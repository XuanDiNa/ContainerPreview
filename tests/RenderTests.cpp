#include "ContainerPreview/NativeRender.h"
#include "ContainerPreview/DiagnosticTrace.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace preview;
namespace {
int checks{},failures{};void Check(bool b,char const* n){++checks;if(!b){++failures;std::cerr<<"FAIL "<<n<<'\n';}}
struct Drawing {std::vector<UiRect> fills;std::vector<std::string> texts;std::vector<void const*> items;bool invalid{},clientIcon{},stableIcon{},throwItem{};};
void Fill(void* p,UiRect const& r,UiColor const&){auto& d=*static_cast<Drawing*>(p);d.fills.push_back(r);d.invalid|=r.right<=r.left || r.bottom<=r.top;}
void Text(void* p,UiRect const&,NativeText& t){static_cast<Drawing*>(p)->texts.emplace_back(t.data,t.length);}
void Item(void* p,void const* s,float,float,float scale){auto& d=*static_cast<Drawing*>(p);d.items.push_back(s);d.invalid|=scale<=0;
    d.clientIcon=static_cast<std::byte const*>(s)[80]==std::byte{0xC1};d.stableIcon=static_cast<std::byte const*>(s)[36]==std::byte{0};
    if(d.throwItem)throw std::runtime_error("draw fault");}
bool Copy(void* dst,void const* src){std::memcpy(dst,src,152);return true;}bool Destroy(void*){return true;}
int clientCopies{},clientDestroys{};bool copyFails{},destroyFails{},invalidClone{};
bool ClientCopy(void* dst,void const* src){++clientCopies;if(copyFails)return false;std::memcpy(dst,src,152);
    static_cast<std::byte*>(dst)[80]=std::byte{0xC1};static_cast<std::byte*>(dst)[36]=std::byte{1};
    if(invalidClone)static_cast<std::byte*>(dst)[34]=std::byte{0};return true;}
bool ClientDestroy(void* dst){++clientDestroys;std::memset(dst,0xEE,152);return !destroyFails;}
struct IconCall {void* renderer{};void* context{};void* stack{};unsigned frame{99};float x{},y{};unsigned pass{99};float alpha{},light{},scale{};int z{-1};bool scoped{};};
IconCall iconCall{};unsigned iconCalls{};bool throwIcon{};
void InventoryDraw(void* r,void* c,void* s,unsigned frame,float x,float y,unsigned char pass,float alpha,float light,float scale,int z){
    ++iconCalls;iconCall={r,c,s,frame,x,y,pass,alpha,light,scale,z,PreviewIconScope::Matches(s)};
    if(throwIcon)throw std::runtime_error("native icon draw fault");
}
}
int main(){
    // Breaks caught: invoking the untextured/effect pass, swapping alpha/light/
    // scale, dropping the client clone, or leaking preview attribution on throw.
    int renderer{},context{},displayStack{},outerStack{};
    DrawInventoryIcon(InventoryDraw,&renderer,&context,&displayStack,23.f,47.f,.875f);
    Check(iconCalls==1 && iconCall.renderer==&renderer && iconCall.context==&context && iconCall.stack==&displayStack,
        "native complete icon entry receives the exact display clone once");
    Check(iconCall.frame==0 && iconCall.pass==0 && iconCall.x==23.f && iconCall.y==47.f && iconCall.alpha==1.f && iconCall.light==1.f && iconCall.scale==.875f && iconCall.z==0,
        "native base-icon ABI preserves coordinates brightness opacity and scale instead of selecting an effect pass");
    Check(iconCall.scoped && !PreviewIconScope::Matches(&displayStack),"complete icon pass is synchronously attributed and scope is released");
    {PreviewIconScope outer(&outerStack);throwIcon=true;
        try{DrawInventoryIcon(InventoryDraw,&renderer,&context,&displayStack,1.f,2.f,1.f);}catch(std::runtime_error const&){}
        Check(iconCalls==2 && iconCall.scoped && PreviewIconScope::Matches(&outerStack) && !PreviewIconScope::Matches(&displayStack),
            "native draw failure restores the previous preview attribution");throwIcon=false;
    }
    NativeText text;Check(MakeText(text,"Preview") && text.length==7 && text.capacity==15 && std::string(text.data)=="Preview","native text uses allocator-independent SSO ABI");
    Check(!MakeText(text,"0123456789012345"),"long text cannot cross game allocator boundary");
    std::array<std::byte,152> stack{};stack[34]=std::byte{64};stack[35]=std::byte{1};OwnedStacks stacks({Copy,Destroy});std::array<void const*,1> items{stack.data()};stacks.Replace(items,1);
    Drawing drawing;RenderSink sink{&drawing,Fill,Text,Item,{ClientCopy,ClientDestroy}};Snapshot snapshot{};snapshot.slots=1;snapshot.pairedServer=true;snapshot.contents=Contents::Ready;
    Check(DrawPreview(sink,Display::Grid,snapshot,stacks,320,180),"known snapshot emits bounded native draw commands");
    Check(drawing.items.size()==1 && drawing.items[0]!=stacks.At(0) && drawing.items[0]!=stack.data() && drawing.clientIcon,
        "GUI resolves a client-registry display clone, not the server-registry snapshot or world stack");
    Check(clientCopies==1 && clientDestroys==1,"each display copy is destroyed in the same synchronous rendering call");
    Check(drawing.stableIcon,"client-side reconstruction cannot restart the pickup animation");
    Check(static_cast<std::byte const*>(stacks.At(0))[80]==std::byte{0} && stack[80]==std::byte{0},"client reconstruction cannot change the snapshot or world stack");
    Check(drawing.texts==std::vector<std::string>{"Preview","64"},"native count is rendered separately from icon");
    Check(!drawing.invalid && drawing.fills.size()==2 && drawing.fills[0].left>=0 && drawing.fills[0].right<=320 && drawing.fills[0].bottom<=180,"panel and slot use correct native rectangle field order");
    Drawing unknown;RenderSink other{&unknown,Fill,Text,Item};Check(DrawPreview(other,Display::Waiting,snapshot,stacks,320,180) && unknown.items.empty() && unknown.texts==std::vector<std::string>{"No data"},"unknown contents never draw an apparently empty inventory");
    Drawing hidden;RenderSink none{&hidden,Fill,Text,Item};Check(!DrawPreview(none,Display::Hidden,snapshot,stacks,320,180) && hidden.fills.empty(),"hidden view produces no draw calls");
    Check(!DrawPreview(sink,Display::Grid,snapshot,stacks,10,10),"unreadable viewport causes no native drawing");
    Drawing missing;RenderSink missingOps{&missing,Fill,Text,Item};
    Check(!DrawPreview(missingOps,Display::Grid,snapshot,stacks,320,180) && missing.items.empty(),"missing display ownership operations cannot fall back to the server item definition");
    Drawing failed;RenderSink failure{&failed,Fill,Text,Item,{ClientCopy,ClientDestroy}};
    copyFails=true;clientCopies=clientDestroys=0;
    Check(!DrawPreview(failure,Display::Grid,snapshot,stacks,320,180) && failed.items.empty() && clientCopies==1 && clientDestroys==0,
        "failed native construction never renders or destroys an unconstructed stack");copyFails=false;
    invalidClone=true;clientCopies=clientDestroys=0;
    Check(!DrawPreview(failure,Display::Grid,snapshot,stacks,320,180) && failed.items.empty() && clientCopies==1 && clientDestroys==1,
        "a client-registry miss is released without drawing the wrong count or an empty icon");invalidClone=false;
    destroyFails=true;clientCopies=clientDestroys=0;
    Check(!DrawPreview(failure,Display::Grid,snapshot,stacks,320,180) && clientCopies==1 && clientDestroys==1,
        "native display destruction failure is propagated to the fail-closed renderer");destroyFails=false;
    Drawing throwing;throwing.throwItem=true;RenderSink throwingSink{&throwing,Fill,Text,Item,{ClientCopy,ClientDestroy}};clientCopies=clientDestroys=0;
    try{DrawPreview(throwingSink,Display::Grid,snapshot,stacks,320,180);}catch(std::runtime_error const&){}
    Check(clientCopies==1 && clientDestroys==1,"a draw exception still releases the constructed display clone");
    Drawing remote;RenderSink remoteSink{&remote,Fill,Text,Item,{ClientCopy,ClientDestroy}};
    snapshot.pairedServer=false;snapshot.remote={400,500,600,700,1,2};
    Check(DrawPreview(remoteSink,Display::Grid,snapshot,stacks,320,180) && remote.items.size()==1,"separately leased remote contents use the accepted icon path without claiming local pairing");
    snapshot.pairedServer=true;
    Check(!DrawPreview(remoteSink,Display::Grid,snapshot,stacks,320,180),"ambiguous mixed integrated and remote sources are rejected");
    snapshot.pairedServer=false;snapshot.remote.id=255;
    Check(!DrawPreview(remoteSink,Display::Grid,snapshot,stacks,320,180),"unaccepted remote lease cannot reach item rendering");
    stacks.Clear(1);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
