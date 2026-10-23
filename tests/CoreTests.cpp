#include "ContainerPreview/PreviewModel.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
using namespace preview;
namespace {
int failures{}, checks{};
void Check(bool value, char const* name) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
Target Chest() { return {100, 200, 300, 0, 10, 64, -1, TargetKind::Block, 0}; }
Eligibility Active() { return {true,true,true,true}; }
Snapshot Sample(Request const& r) { return {r.target,r.generation,1100,true,Contents::Ready,27}; }
void ConfigTests() {
    Check(ParseConfig("{}") == Config{}, "empty JSON uses enabled Left Alt defaults");
    Check(ParseConfig(R"({"enabled":false,"holdKey":"F5"})") == Config{false,0x74}, "custom key and disabled config are applied");
    Check(ParseConfig(R"({"holdKey":"RALT"})") == Config{true,0xA5}, "right Alt remains distinct from left Alt");
    for (auto text : {"[]", "null", "{", R"({"enabled":1})", R"({"holdKey":0})", R"({"holdKey":"NO_KEY"})", R"({"enabled":false,"holdKey":"NO_KEY"})"}) {
        Config c{true,0xA4};
        Check(!ReloadConfig(c,text) && c==Config{}, "invalid edit retains entire prior config");
    }
    Config c;
    Check(ReloadConfig(c,R"({"holdKey":"G"})") && c.holdKey==0x47, "valid edit reloads config");
}
void LifecycleTests() {
    PreviewModel m; Config c; auto t=Chest(); auto active=Active();
    m.Update(c,active,t,1000); auto r=m.Current(1000);
    Check(r.has_value(), "holding key in local HUD publishes request");
    if (!r) return;
    auto s=Sample(*r);
    Check(m.View(s,1100)==Display::Grid, "paired fresh known contents display grid");
    s.pairedServer=false;
    Check(m.View(s,1100)==Display::Waiting, "remote or unpaired data never displays as local contents");
    s=Sample(*r);s.contents=Contents::Unknown;
    Check(m.View(s,1100)==Display::Waiting, "unknown contents are not an empty container");
    s=Sample(*r);s.contents=Contents::Absent;
    Check(m.View(s,1100)==Display::Hidden,"known non-container targets never show unavailable preview");
    s=Sample(*r);s.slots=0;
    Check(m.View(s,1100)==Display::Waiting, "zero-sized invalid container does not display an empty grid");
    s.slots=55;
    Check(m.View(s,1100)==Display::Waiting, "oversized native inventory is rejected");
    s=Sample(*r);
    m.Update(c,active,t,1200);
    Check(m.Current(1200)->generation==r->generation, "same target refresh does not churn generation");
    Check(m.View(s,1601)==Display::Waiting, "snapshot older than 500 ms is hidden");
    Check(!m.Current(1701), "client request older than 500 ms expires");
    Check(!m.Current(1199), "clock reversal invalidates request");
    t.x=11;m.Update(c,active,t,1300);
    Check(m.Current(1300)->generation!=r->generation && m.View(s,1300)==Display::Waiting, "target switch invalidates previous snapshot");
    auto second=*m.Current(1300);
    auto released=active;released.held=false;m.Update(c,released,t,1400);
    Check(!m.Current(1400) && m.View(s,1400)==Display::Hidden, "release hides immediately");
    m.Update(c,active,t,1450);
    Check(m.Current(1450)->generation!=second.generation, "re-hold never resurrects old generation");
    for (int gate=0;gate<5;++gate) {
        m.Update(c,active,t,1500);
        auto e=active;auto cfg=c;
        if(gate==0)e.foreground=false;if(gate==1)e.hud=false;if(gate==2)e.local=false;if(gate==3)e.held=false;if(gate==4)cfg.enabled=false;
        m.Update(cfg,e,t,1501);
        Check(!m.Current(1501), "focus/menu/remote/release/disabled gate invalidates request");
    }
    for(auto kind:{TargetKind::None,TargetKind::Player}) {
        t=Chest();t.kind=kind;m.Update(c,active,t,1600);
        Check(!m.Current(1600), "misses and players are excluded");
    }
    for(int field=0;field<3;++field) {
        t=Chest();if(field==0)t.world=0;if(field==1)t.unique=0;if(field==2)t.runtime=0;
        m.Update(c,active,t,1650);Check(!m.Current(1650), "incomplete local identity fails closed");
    }
    t=Chest();m.Update(c,active,t,1700);auto old=*m.Current(1700);auto before=Sample(old);before.time=1700;
    t.world=101;m.Update(c,active,t,1701);
    Check(m.View(before,1701)==Display::Waiting, "world replacement invalidates stale data even at same position");
    auto now=*m.Current(1701);auto other=Sample(now);other.time=1701;other.target.dimension=1;
    Check(m.View(other,1701)==Display::Waiting, "dimension mismatch rejects snapshot");
    other=Sample(now);other.time=1702;
    Check(m.View(other,1701)==Display::Waiting, "future-dated snapshot rejected");
    t=Chest();t.kind=TargetKind::Entity;t.entity=400;t.entityRuntime=0;m.Update(c,active,t,1800);
    Check(!m.Current(1800),"entity unique ID without runtime ID is not a valid target");
    t.entityRuntime=500;m.Update(c,active,t,1801);Check(m.Current(1801).has_value(),"complete non-player entity identity admits request");
    auto entityRequest=*m.Current(1801);auto entitySample=Sample(entityRequest);entitySample.time=1801;
    t.entityRuntime=501;m.Update(c,active,t,1802);Check(m.View(entitySample,1802)==Display::Waiting,"reused entity unique ID cannot revive old runtime snapshot");
}
void LayoutTests() {
    auto g=LayoutGrid(54,320,180);
    Check(g && g->columns==9 && g->rows==6 && g->cell>=8, "double chest layout has bounded nine-column grid");
    if(g)Check(g->x>=0 && g->y>=0 && g->x+g->width<=320 && g->y+g->height<=180, "panel remains inside viewport");
    g=LayoutGrid(3,320,180);Check(g && g->columns==3 && g->rows==1, "small inventories avoid empty nine-column panel");
    g=LayoutGrid(1,320,180);Check(g && g->width>=64,"one slot panel still contains readable short title");
    for(auto slots:{0u,55u,UINT_MAX})Check(!LayoutGrid(slots,320,180), "invalid slot count rejected before arithmetic");
    Check(!LayoutGrid(27,10,10), "unreadably small viewport rejected");
    Check(!LayoutGrid(27,std::numeric_limits<float>::infinity(),180), "nonfinite viewport rejected");
}
}
int main() {
    ConfigTests();LifecycleTests();LayoutTests();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
