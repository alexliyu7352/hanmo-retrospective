// 仅检验已验证事实到本人见闻的只读适配；这里不代完整军训World门。
#include "resident/domains/growth/growth_insight_sources.hpp"
#include <algorithm>
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
void check(bool pass,const char* label){++checks;if(!pass){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
MilitaryTrainingFact complete(const StableId& id,Integer end){
    MilitaryTrainingFact fact;fact.id=id;fact.actor_id="instructor";fact.phase="completed";
    fact.success=true;fact.start_tick=end-4;fact.completed_tick=end;fact.lessons.resize(4);
    fact.terms.actor_id="instructor";fact.terms.participants={"instructor","player"};
    fact.terms.garrison_id="camp";fact.terms.place_id="camp-yard";fact.terms.unit_id="unit";
    fact.terms.authority_source="instructor-appointment";return fact;
}
}
int main(){
    CommonState common;common.identity.world_id[0]=1;common.identity.tick=20;
    common.metadata.player_actor_id="player";common.metadata.scenario_id="camp-life";
    ActorRecord player;player.id="player";common.actors.insert(player.id,player);
    MilitaryState military;auto read=[&](const MilitaryState& state){return first_growth_camp_training_experience(common,state,"player");};
    auto empty=read(military);check(empty.ok&&!empty.value,"being present alone grants no training experience");
    military.training_facts.push_back(complete("lesson:later",12));auto earned=read(military);
    check(earned.ok&&earned.value&&earned.value->actor_id=="player"&&earned.value->fact_id=="lesson:later","participant receives own experience even when instructor issued command");
    if(!earned.ok||!earned.value)return 1;
    check(earned.value->garrison_id=="camp"&&earned.value->place_id=="camp-yard"&&earned.value->unit_id=="unit"&&earned.value->started_tick==8&&earned.value->completed_tick==12,"actual frozen camp and supplied course time retained");
    check(earned.value->world_id==common.identity.world_id&&earned.value->scenario_id==common.metadata.scenario_id,"permanent experience binds same world and scenario");
    auto changed=military;changed.training_facts[0].terms.participants={"instructor"};changed.training_facts[0].terms.fit_ids={"player"};
    auto absent=read(changed);check(absent.ok&&!absent.value,"fit roster alone is not proof of a real Actor participant");
    changed.training_facts[0].actor_id="player";absent=read(changed);check(absent.ok&&!absent.value,"command actor alone does not bypass participant requirement");
    changed=military;changed.training_facts[0].phase="started";changed.training_facts[0].success=false;absent=read(changed);check(absent.ok&&!absent.value,"unfinished course is no completed experience");
    changed=military;changed.training_facts[0].success=false;changed.training_facts[0].code="TrainingInterrupted";absent=read(changed);check(absent.ok&&!absent.value,"interrupted course is not quietly recorded as completed");
    changed=military;changed.training_facts[0].lessons.resize(3);absent=read(changed);check(absent.ok&&!absent.value,"partial lessons do not qualify");
    military.training_facts.push_back(complete("lesson:b",8));military.training_facts.push_back(complete("lesson:a",8));earned=read(military);
    check(earned.ok&&earned.value&&earned.value->fact_id=="lesson:a"&&earned.value->completed_tick==8,"first completed course is stable by time then fact ID");
    std::reverse(military.training_facts.begin(),military.training_facts.end());auto again=read(military);check(again.ok&&again.value&&again.value->fact_id=="lesson:a","storage order and repeated viewing cannot reroll first source");
    changed=military;changed.training_facts.push_back(changed.training_facts.front());check(!read(changed).ok,"duplicated matching facts are refused");
    changed=military;changed.training_facts.front().completed_tick=21;check(!read(changed).ok,"future evidence cannot appear in current world");
    check(!first_growth_camp_training_experience(common,military,"instructor").ok,"viewer cannot inspect another actor by guessing ID");
    std::cout<<"GROWTH_INSIGHT_SOURCES "<<checks<<" checks "<<failures<<" failures; COMPONENT_ONLY\n";
    return failures?1:0;
}
