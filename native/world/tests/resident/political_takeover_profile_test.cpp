// 新接管三域必须完整成套；旧情景不因一个空侧账被静默升级。
#include "resident/domains/politics/takeover/politics_takeover_profile.hpp"
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
}
int main(){
    PoliticalTakeoverState state;
    const std::vector<std::pair<StableId,StableId>> pair{{"political_takeover","1"},{"economic_takeover_execution","1"}};
    check(!validate_political_takeover_layout(true,pair,&state,1).ok,"missing task execution sidecar rejected");
    auto complete=pair;complete.emplace_back("task_takeover_execution","1");
    check(validate_political_takeover_layout(true,complete,&state,1,1).ok,"all three declared module and state versions accepted");
    check(!validate_political_takeover_layout(true,complete,&state,1).ok,"declared task module without state rejected");
    check(!validate_political_takeover_layout(true,complete,nullptr,1,1).ok,"missing political state rejected");
    check(!validate_political_takeover_layout(true,complete,&state,{},1).ok,"missing economic execution state rejected");
    check(!validate_political_takeover_layout(true,complete,&state,1,2).ok,"unknown task execution state version rejected");
    complete.back().second="2";check(!validate_political_takeover_layout(true,complete,&state,1,1).ok,"unknown task module version rejected");complete.back().second="1";
    complete.push_back(complete.back());check(!validate_political_takeover_layout(true,complete,&state,1,1).ok,"duplicate task module rejected");complete.pop_back();
    check(validate_political_takeover_layout(false,{},nullptr).ok,"unmodified old profile accepted");
    check(!validate_political_takeover_layout(false,{},nullptr,{},1).ok,"hidden task sidecar in old profile rejected");
    check(!validate_political_takeover_layout(false,complete,&state,1,1).ok,"disabled takeover rejects declared new state");
    std::cout<<"political takeover profile: "<<checks<<" checks, 0 failures\n";
}
