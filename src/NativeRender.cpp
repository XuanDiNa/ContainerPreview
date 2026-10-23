#include "ContainerPreview/NativeRender.h"
#include "ContainerPreview/DiagnosticTrace.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <charconv>
#include <cstring>
#include <cmath>
namespace preview {
namespace {
bool DrawDisplayItem(RenderSink const& sink,void const* source,float x,float y,float scale){
    if(!sink.itemCopies.copy || !sink.itemCopies.destroy)return false;
    alignas(8) std::array<std::byte,152> display{};
    bool constructed=false,ok=false;
    __try {
        if(sink.itemCopies.copy(display.data(),source)){
            constructed=true;
            auto bytes=static_cast<std::byte const*>(source);
            // Native construction remaps Item IDs through the current registry.
            // A missing client item must not silently render as an empty stack.
            if(display[35]==std::byte{1} && display[34]==bytes[34]){
                display[36]=std::byte{0};
                sink.item(sink.user,display.data(),x,y,scale);ok=true;
            }
        }
    }__finally {
        if(constructed && !sink.itemCopies.destroy(display.data()))ok=false;
    }
    return ok;
}
}
bool MakeText(NativeText& text,std::string_view value){
    if(value.size()>15)return false;
    text={};std::memcpy(text.data,value.data(),value.size());text.length=value.size();return true;
}
void DrawInventoryIcon(InventoryItemDraw draw,void* renderer,void* actorContext,void const* stack,float x,float y,float scale){
    PreviewIconScope previewScope(stack);
    // 5568B00 base pass calls the full 55AC5B0 entry with this eleven-arg ABI.
    // It selects terrain/items/special textures before drawing flat icons;
    // 55AD170 alone may deliberately submit an untextured effect quad.
    draw(renderer,actorContext,const_cast<void*>(stack),0,x,y,0,1.f,1.f,scale,0);
}
bool DrawPreview(RenderSink const& sink,Display display,Snapshot const& snapshot,OwnedStacks const& stacks,float width,float height){
    if(display==Display::Hidden || !sink.fill || !sink.text || !sink.item)return false;
    auto grid=LayoutGrid(display==Display::Grid?snapshot.slots:1,width,height);if(!grid)return false;
    if(display==Display::Grid && (stacks.Size()!=snapshot.slots || !HasPreviewSource(snapshot) || snapshot.contents!=Contents::Ready))return false;
    auto const& g=*grid;UiRect panel{g.x,g.x+g.width,g.y,g.y+g.height};sink.fill(sink.user,panel,{.08f,.06f,.12f,.94f});
    NativeText title;MakeText(title,display==Display::Grid?"Preview":"No data");
    UiRect titleRect{g.x+4,panel.right-2,g.y+3,g.y+15};sink.text(sink.user,titleRect,title);
    if(display==Display::Waiting)return true;
    for(unsigned slot=0;slot<snapshot.slots;++slot){
        float x=g.x+4+(slot%g.columns)*g.cell,y=g.y+16+(slot/g.columns)*g.cell;
        sink.fill(sink.user,{x,x+g.cell-1,y,y+g.cell-1},{.22f,.2f,.25f,.95f});
        auto stack=static_cast<std::byte const*>(stacks.At(slot));if(!stack)return false;
        auto count=std::to_integer<unsigned>(stack[34]);
        if(stack[35]!=std::byte{1} || !count)continue;
        if(!DrawDisplayItem(sink,stack,x+2,y+2,(g.cell-4)/16.f))return false;
        if(count>1){
            char digits[4]{};auto end=std::to_chars(digits,digits+3,count).ptr;NativeText text;MakeText(text,{digits,static_cast<std::size_t>(end-digits)});
            float left=x+g.cell-2-static_cast<float>(text.length)*6;
            sink.text(sink.user,{left,x+g.cell,y+g.cell-9,y+g.cell},text);
        }
    }
    return true;
}
namespace {
template<class T>T Read(void const* p,std::size_t offset){T value;std::memcpy(&value,static_cast<std::byte const*>(p)+offset,sizeof value);return value;}
struct TextMeasure {float scale{1},padding{};bool shadow{true},colors{},hyphen{};};
struct Caret {int position{-1};bool render{};};
static_assert(sizeof(TextMeasure)==12 && sizeof(Caret)==8);
struct NativeDrawing {NativeProfile const* profile;void* context;void* renderer;void* actorContext;};
void Fill(void* user,UiRect const& rect,UiColor const& color){
    auto& draw=*static_cast<NativeDrawing*>(user);
    draw.profile->Function<void(*)(void*,UiRect const*,UiColor const*,float)>(0x6636EE0)(draw.context,&rect,&color,color.a);
}
void Text(void* user,UiRect const& rect,NativeText& value){
    auto& draw=*static_cast<NativeDrawing*>(user);UiColor white{1,1,1,1};TextMeasure measure;Caret caret;
    draw.profile->Function<void(*)(void*,UiRect const*,NativeText*,UiColor const*,float,int,TextMeasure const*,Caret const*)>(0x662FED0)(draw.context,&rect,&value,&white,1.f,0,&measure,&caret);
}
void Item(void* user,void const* stack,float x,float y,float scale){
    auto& draw=*static_cast<NativeDrawing*>(user);
    DrawInventoryIcon(draw.profile->Function<InventoryItemDraw>(0x55AC5B0),draw.renderer,draw.actorContext,stack,x,y,scale);
}
}
bool RenderNative(NativeProfile const& p,void* context,Display display,Snapshot const& snapshot,OwnedStacks const& stacks,StackOps copies) noexcept{
    if(display==Display::Hidden)return true;
    alignas(16) std::array<std::byte,192> actorContext{};
    alignas(8) std::array<std::byte,16> registry{};
    alignas(8) std::array<std::byte,24> registryScope{};
    std::array<std::byte,17> previousSave{};bool clipped=false,constructed=false,scoped=false,ok=false;
    __try {
        if(!p.Table(context,0xE9958B0))return false;
        auto client=Read<void*>(context,8),screen=Read<void*>(context,16);
        if(!p.Table(client,0xE9731B0) || !screen)return false;
        auto resourceGetter=p.Virtual(client,632);if(!resourceGetter)return false;
        auto resource=reinterpret_cast<void*(*)(void*)>(resourceGetter)(client);
        UiRect viewport{};p.Function<void*(*)(void*,UiRect*)>(0x6637C10)(context,&viewport);
        if(!LayoutGrid(display==Display::Grid?snapshot.slots:1,viewport.right,viewport.bottom))return true;
        std::memcpy(previousSave.data(),static_cast<std::byte const*>(context)+296,17);
        p.Function<void(*)(void*)>(0x6637B00)(context);clipped=true;
        p.Function<void(*)(void*)>(0x6637AF0)(context);
        p.Function<void*(*)(void*,void*,void*,void*)>(0x1ED15B0)(actorContext.data(),screen,client,resource);constructed=true;
        auto renderer=Read<void*>(actorContext.data(),88);
        if(renderer){
            // Match the native UI's registry scope (4460630 / 5DA62B0).
            // 3489AD0 consumes the getter's weak pair. 3489C20 restores the
            // previous thread registry and releases the saved weak reference.
            auto registryGetter=p.GuardedMethod<void*(*)(void*,void*)>(0x5DD4520,client,0xE9731B0);
            if(registryGetter){
                registryGetter(client,registry.data());
                p.Function<void*(*)(void*,void*)>(0x3489AD0)(registryScope.data(),registry.data());scoped=true;
                auto control=Read<void*>(registry.data(),8);
                if(Read<void*>(registry.data(),0) && control && Read<int>(control,8)>0 && registryScope[0]==std::byte{1}){
                    NativeDrawing drawing{&p,context,renderer,actorContext.data()};RenderSink sink{&drawing,Fill,Text,Item,copies};
                    ok=DrawPreview(sink,display,snapshot,stacks,viewport.right,viewport.bottom);
                }
            }
            // std::optional<float> is passed as an eight-byte value in R8;
            // bit32 is engaged (verified by 6630150 assembly). Empty is zero.
            p.Function<void(*)(void*,float,std::uint64_t)>(0x6630150)(context,0.f,0);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    __try {
        if(scoped)p.Function<void(*)(void*)>(0x3489C20)(registryScope.data());
    }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    __try {
        if(constructed)p.Function<void(*)(void*)>(0x1ED1A60)(actorContext.data());
        if(clipped){
            p.Function<void(*)(void*)>(0x6637B90)(context);
            std::memcpy(static_cast<std::byte*>(context)+296,previousSave.data(),17);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    return ok;
}
}
