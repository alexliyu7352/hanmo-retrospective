// 有限DTO测试确认读取成本受64行上限约束且页不借用可变源。
#include "resident/views/journey_view.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main() {
    WorldIdentity id; id.world_id[0]=1; id.execution_epoch=2; id.revision=8; int reads=0; std::string text="before";
    auto read=[&](Integer i)->Outcome<JourneyViewRow>{++reads; return {true,{std::to_string(i),"route",text,PointI{i,2}}, {}};};
    auto page=make_journey_page(id,"player",0,100000,read); assert(page.ok&&page.value->rows.size()==64&&reads==64&&page.value->next_cursor==64);
    text="after"; assert(page.value->rows[0].text=="before"&&page.value->identity.execution_epoch==2);
    auto last=make_journey_page(id,"player",99999,100000,read); assert(last.ok&&last.value->rows.size()==1&&!last.value->next_cursor);
    assert(!make_journey_page(id,"player",-1,1,read).ok); assert(!make_journey_page(id,"player",2,1,read).ok);
    text.assign(4097,'x'); assert(!make_journey_page(id,"player",0,1,read).ok);
    std::cout<<"finite journey view checks passed\n";
}
