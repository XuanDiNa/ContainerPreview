#include "ContainerPreview/Config.h"
#include "../include/sdk/ModMenuBinding.h"
#include "ContainerPreview/DiagnosticTrace.h"
#include "ContainerPreview/HookSet.h"
#include "ContainerPreview/NativeRender.h"
#include "ContainerPreview/NativeWorld.h"
#include "ContainerPreview/NativeRemote.h"
#include "ContainerPreview/HeldUse.h"
#include "ContainerPreview/EntityUse.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <algorithm>
#include <climits>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
namespace {
using namespace preview;
HINSTANCE module{};NativeProfile profile;
std::atomic<int> status{};std::atomic<bool> started{},nativeFault{};
std::atomic<unsigned> settings{1u|(0xA4u<<1)};
std::atomic<std::uint64_t> clientCalls{},serverCalls{},requests{},paired{},snapshots{},draws{};
std::atomic<std::uint64_t> heldCalls{},observations{},renderCalls{},renderHeld{},renderViews{},renderGated{};
std::filesystem::path directory;
using ClientUpdate=std::uint64_t(*)(void*,int);
using LevelTick=std::uint64_t(*)(void*);
using ScreenRender=std::uint64_t(*)(void*,void*);
ClientUpdate clientOriginal{};LevelTick tickOriginal{};ScreenRender renderOriginal{};
std::atomic<std::uint32_t> nextOwner{1};
std::uint32_t Owner(){thread_local std::uint32_t token=nextOwner.fetch_add(1);return token;}
bool CopyStack(void* out,void const* source){
    __try {profile.Function<void*(*)(void*,void const*)>(0x2482180)(out,source);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){nativeFault.store(true);return false;}
}
bool DestroyStack(void* value){
    __try {profile.Function<void*(*)(void*,int)>(0x21B1C0)(value,0);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){nativeFault.store(true);return false;}
}
// Deliberately pinned: no native calls, thread-affine destruction or unhooking
// occur from DllMain/DLL_PROCESS_DETACH.
PreviewBridge* bridge{};
Config Settings(){auto s=settings.load(std::memory_order_acquire);return {bool(s&1),(s>>1)&255u,(s>>9)&7u};}
bool Foreground(){DWORD pid{};GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
bool Held(Config const& c){
    thread_local modmenu::binding::Tracker tracker;
    modmenu::binding::Binding binding{c.holdKey,c.holdModifiers};
    auto sample=modmenu::binding::Sample(binding,[](unsigned k){return (GetAsyncKeyState(static_cast<int>(k))&0x8000)!=0;});
    return tracker.Update(binding,sample,c.enabled&&Foreground()).held;
}
bool ShiftHeld(){return (GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;}
#include "DiagnosticHooks.inl"
std::uint64_t ClientHook(void* client,int flag){
    DiagnosticScope clientScope(TraceStage::Client,reinterpret_cast<std::uintptr_t>(client));
    return ForwardOnce(clientOriginal,[&]{
        ++clientCalls;if(status.load(std::memory_order_acquire)!=1 || !bridge)return;
        auto config=Settings();bool foreground=Foreground(),held=Held(config);
        ClientObservation observation{};
        if(config.enabled && foreground && held){++heldCalls;observation=ObserveClient(profile,client);if(observation.hud)++observations;}
        TraceClientSample(clientScope,client,observation,held,foreground);
        auto now=GetTickCount64();
        bridge->Update(config,{foreground,observation.hud,held,observation.identity.local},observation.target,observation.identity,now);
        RemoteTick(client,observation,Held(config),now);
        if(observation.target.kind!=TargetKind::None)++requests;
    },client,flag);
}
std::uint64_t TickHook(void* level){
    return ForwardOnce(tickOriginal,[&]{
        ++serverCalls;if(status.load(std::memory_order_acquire)!=1 || !bridge || !profile.Table(level,0xE83B2B0))return;
        auto owner=Owner();auto now=GetTickCount64();bridge->Cleanup(owner,now);
        auto work=bridge->Work(now);if(!work)return;
        auto result=ObserveServer(profile,level,*work);if(!result.paired)return;++paired;
        if(bridge->Publish(*work,result.identity,reinterpret_cast<std::uintptr_t>(level),result.contents,owner,now) && result.contents.ready)++snapshots;
    },level);
}
bool RenderGate(void* context,std::uint64_t expectedWorld){
    __try {
        if(!profile.Table(context,0xE9958B0))return false;
        auto client=*reinterpret_cast<void**>(static_cast<std::byte*>(context)+8);
        auto getter=profile.GuardedMethod<void*(*)(void*)>(0x5DC2630,client,0xE9731B0);
        if(!getter)return false;
        auto world=getter(client);
        return reinterpret_cast<std::uintptr_t>(world)==expectedWorld && NativeHud(profile,client);
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
std::uint64_t RenderHook(void* screen,void* context){
    return ForwardOnce(renderOriginal,[&]{
        ++renderCalls;
        if(status.load(std::memory_order_acquire)!=1 || !bridge)return;
        auto config=Settings();if(!Held(config))return;
        ++renderHeld;
        bridge->View(GetTickCount64(),[&](Display display,Snapshot const& snapshot,OwnedStacks const& stacks){
            ++renderViews;
            if(display==Display::Hidden || !HasPreviewSource(snapshot) || !RenderGate(context,snapshot.target.world))return;
            ++renderGated;
            if(RenderNative(profile,context,display,snapshot,stacks,{CopyStack,DestroyStack}))++draws;
            else nativeFault.store(true);
        });
    },screen,context);
}
void Log(std::string const& line){
    std::ofstream out(directory/"ContainerPreview.log",std::ios::app);
    out<<GetTickCount64()<<" "<<line<<"\n";
}
int Create(void* p,void* d,void** o){return MH_CreateHook(p,d,o);}
int Enable(void* p){return MH_EnableHook(p);}int Disable(void* p){return MH_DisableHook(p);}int Remove(void* p){return MH_RemoveHook(p);}
bool Hooks(){
    auto result=MH_Initialize();
    if(result!=MH_OK && result!=MH_ERROR_ALREADY_INITIALIZED){
        Log("Hook installation failed: stage=initialize; backendStatus="+std::to_string(static_cast<int>(result))+"; "+MH_StatusToString(result));
        return false;
    }
    auto old=CurrentVersion()==GameVersion::V45;
    HookSpec hooks[]={
        {reinterpret_cast<void*>(profile.Base()+Rva(0x5DA7C00)),reinterpret_cast<void*>(ClientHook),reinterpret_cast<void**>(&clientOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1171390)),reinterpret_cast<void*>(TickHook),reinterpret_cast<void**>(&tickOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x4839060)),reinterpret_cast<void*>(RenderHook),reinterpret_cast<void**>(&renderOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x13423C0)),reinterpret_cast<void*>(OpenHook),reinterpret_cast<void**>(&openOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1344240)),reinterpret_cast<void*>(ContentHook),reinterpret_cast<void**>(&contentOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1342D50)),reinterpret_cast<void*>(SlotHook),reinterpret_cast<void**>(&slotOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1342710)),reinterpret_cast<void*>(ClosePacketHook),reinterpret_cast<void**>(&closePacketOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x136F640)),reinterpret_cast<void*>(DeferredHook),reinterpret_cast<void**>(&deferredOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x673EB20)),reinterpret_cast<void*>(SceneHook),reinterpret_cast<void**>(&sceneOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x6772D10)),reinterpret_cast<void*>(FactoryHook),reinterpret_cast<void**>(&factoryOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x47614C0)),reinterpret_cast<void*>(ClosePlayerHook),reinterpret_cast<void**>(&closePlayerOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1EEE80)),reinterpret_cast<void*>(ModelBindingHook),reinterpret_cast<void**>(&modelBindingOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x55AD170)),reinterpret_cast<void*>(GuiItemHook),reinterpret_cast<void**>(&guiItemOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1B87180)),reinterpret_cast<void*>(IconResolveHook),reinterpret_cast<void**>(&iconResolveOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x259BC60)),reinterpret_cast<void*>(BlockUseHook),reinterpret_cast<void**>(&blockUseOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x343EA70)),reinterpret_cast<void*>(ModelApplyHook),reinterpret_cast<void**>(&modelApplyOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x4F0340)),old?reinterpret_cast<void*>(ScreenBuild45Hook):reinterpret_cast<void*>(ScreenBuildHook),old?reinterpret_cast<void**>(&screenBuild45Original):reinterpret_cast<void**>(&screenBuildOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x4DF7B50)),reinterpret_cast<void*>(ControllerDestroyHook),reinterpret_cast<void**>(&controllerDestroyOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x67721D0)),old?reinterpret_cast<void*>(Creator45Hook):reinterpret_cast<void*>(CreatorHook),old?reinterpret_cast<void**>(&creator45Original):reinterpret_cast<void**>(&creatorOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x4B29A50)),reinterpret_cast<void*>(ScenePendingHook),reinterpret_cast<void**>(&scenePendingOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x2598F80)),reinterpret_cast<void*>(EntityUseHook),reinterpret_cast<void**>(&entityUseOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x24C92E0)),reinterpret_cast<void*>(ItemAdjustHook),reinterpret_cast<void**>(&itemAdjustOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x2483000)),old?reinterpret_cast<void*>(ItemUseOn45Hook):reinterpret_cast<void*>(ItemUseOnHook),old?reinterpret_cast<void**>(&itemUseOn45Original):reinterpret_cast<void**>(&itemUseOnOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x2486D00)),reinterpret_cast<void*>(StackEmptyHook),reinterpret_cast<void**>(&stackEmptyOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x6772A80)),reinterpret_cast<void*>(EntityFactoryHook),reinterpret_cast<void**>(&entityFactoryOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1176930)),reinterpret_cast<void*>(ActorLookupHook),reinterpret_cast<void**>(&actorLookupOriginal)},
        {reinterpret_cast<void*>(profile.Base()+Rva(0x1A2B270)),old?reinterpret_cast<void*>(EntityPlan45Hook):reinterpret_cast<void*>(EntityPlanHook),old?reinterpret_cast<void**>(&entityPlan45Original):reinterpret_cast<void**>(&entityPlanOriginal)}
    };
    static_assert(std::size(hooks)<=MaxHookCount,"Hook set exceeds installer capacity");
    HookDiagnostic diagnostic;
    if(InstallHookSet(hooks,{Create,Enable,Disable,Remove},&diagnostic))return true;
    Log("Hook installation failed: "+DescribeHookDiagnostic(diagnostic,profile.Base()));
    return false;
}
DWORD WINAPI Worker(void*){
    try {
        HMODULE pinned{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&Worker),&pinned)){status.store(-1);return 0;}
        wchar_t path[32768]{};auto size=GetModuleFileNameW(module,path,32768);if(!size || size>=32768){status.store(-1);return 0;}
        directory=std::filesystem::path(path).parent_path();
        Log("0.1.1-test5-compat starting; keyboard binding contract2; reviewed26.51 launcher; friend-route/test32; exact Bedrock 1.26.45.01 / 1.26.51.01 / 1.26.52.03; owned address-kind3 remote sessions; separate45 native ABI and layouts; runtime seams remain exact; Shift/native-sneak admission pause before background use; unsubmitted-only recovery; held-item Hive preview policy preserved; returned owner is not a unique request correlation token");
        ProfileDiagnostic diagnostic;
        if(!profile.Initialize(GetModuleHandleW(nullptr),&diagnostic)){status.store(-1);Log("Inactive: "+DescribeProfileDiagnostic(diagnostic));return 0;}
        Log(std::string("Image check: ")+VersionName(CurrentVersion())+"; "+DescribeProfileDiagnostic(diagnostic));
        bridge=new PreviewBridge({CopyStack,DestroyStack});
        diagnosticBuffer=new DiagnosticBuffer;
        Config config;std::string previous;bool seen=false;
        auto reload=[&]{
            std::ifstream file(directory/"config.json",std::ios::binary|std::ios::ate);if(!file)return;
            auto length=file.tellg();if(length<0 || length>4096){if(previous!="<oversize>")Log("Config rejected: file exceeds 4096 bytes; retaining last valid settings");previous="<oversize>";seen=true;return;}
            std::string text(static_cast<std::size_t>(length),'\0');file.seekg(0);file.read(text.data(),length);
            if(seen && text==previous)return;previous=text;seen=true;
            if(!ReloadConfig(config,text)){Log("Config rejected; retaining last valid settings");return;}
            settings.store(unsigned(config.enabled)|(config.holdKey<<1)|(config.holdModifiers<<9),std::memory_order_release);
            Log(std::string("Config loaded: enabled=")+(config.enabled?"true":"false")+", holdKey="+std::to_string(config.holdKey));
        };
        reload();
        if(!Hooks()){status.store(-1);Log("Inactive: guarded hook installation failed; originals preserved");return 0;}
        auto guards=NativeGuards();
        auto loadedGuards=std::count_if(guards.begin(),guards.end(),[](auto const& g){return g.scope==EntryGuardScope::LoadedRequired;});
        status.store(1,std::memory_order_release);Log("Ready: 27 guarded hooks; "+std::to_string(guards.size())+" disk entry guards; "+std::to_string(loadedGuards)+" loaded entry guards; Shift/native-sneak pauses new background use; submitted timeout attribution retained; experimental held-item Hive preview uses verified native proxy; full native content required; manual interaction priority; plugin never transfers items");
        std::uint64_t last=GetTickCount64();
        for(;;){
            if(nativeFault.load()){status.store(-1,std::memory_order_release);Log("Inactive: native snapshot/render invariant failed; hooks now only forward original calls");return 0;}
            reload();auto now=GetTickCount64();
            DrainDiagnostic();
            if(now-last>=5000){
                // Test-build heartbeat includes zero requests. Silence must not
                // conflate a missing hook, missing key, failed HUD gate or data.
                Log("Preview counters: client="+std::to_string(clientCalls.load())+", held="+std::to_string(heldCalls.load())+
                    ", observed="+std::to_string(observations.load())+", level="+std::to_string(serverCalls.load())+
                    ", requests="+std::to_string(requests.load())+", paired="+std::to_string(paired.load())+
                    ", snapshots="+std::to_string(snapshots.load())+", render="+std::to_string(renderCalls.load())+
                    ", renderHeld="+std::to_string(renderHeld.load())+", views="+std::to_string(renderViews.load())+
                    ", gated="+std::to_string(renderGated.load())+", draws="+std::to_string(draws.load())+
                    ", traceDropped="+std::to_string(diagnosticBuffer->Dropped())+
                    ", remoteOpen="+std::to_string(remoteOpens.load())+", remoteCreate="+std::to_string(remoteCreates.load())+
                    ", remoteAccept="+std::to_string(remoteAccepts.load())+", remoteFull="+std::to_string(remoteFull.load())+
                    ", remoteCopy="+std::to_string(remoteCopies.load())+", remoteClose="+std::to_string(remoteCloses.load())+
                    ", remoteManual="+std::to_string(remoteManual.load())+
                    ", remoteConsistency="+std::to_string(remoteConsistency.load()));
                last=now;
            }
            Sleep(200);
        }
    }catch(...){status.store(-1,std::memory_order_release);if(!directory.empty())Log("Inactive: initialization/worker exception");}
    return 0;
}
}
extern "C" __declspec(dllexport) int ContainerPreviewStatus(){return status.load(std::memory_order_acquire);}
extern "C" __declspec(dllexport) int ContainerPreviewInit(HINSTANCE instance){
    if(started.exchange(true))return status.load(std::memory_order_acquire)>=0;
    module=instance;auto thread=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);
    if(!thread){status.store(-1,std::memory_order_release);return 0;}CloseHandle(thread);return 1;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);ContainerPreviewInit(instance);}return TRUE;
}
