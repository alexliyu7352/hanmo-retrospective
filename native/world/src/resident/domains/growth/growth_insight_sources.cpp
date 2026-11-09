#include "resident/domains/military/military_model.hpp"
#include "growth_insight_sources.hpp"
#include <algorithm>
#include <set>

namespace hanmo::resident {
Outcome<std::optional<GrowthCampTrainingExperience>> read_verified_growth_camp_training_experience(
    const CommonState& common,const MilitaryState& military,const StableId& actor){
    using Result=Outcome<std::optional<GrowthCampTrainingExperience>>;
    auto fail=[](const StableId& code,const StableId& path=StableId{})->Result{
        return {false,{},common_failure(code,path).error};
    };
    if(actor.empty())return fail("UnknownActor",actor);
    if(!common.actors.find(actor))return fail("UnknownActor",actor);
    if(common.identity.tick<0||common.metadata.scenario_id.empty()||
       std::all_of(common.identity.world_id.begin(),common.identity.world_id.end(),[](auto byte){return byte==0;}))
        return fail("InvalidInsightWorld");
    std::optional<GrowthCampTrainingExperience> first;
    for(const auto& fact:military.training_facts){
        // 完成和完整供给/费用的正确性仍归军事领域；这里不重算其规则。
        if(fact.phase!="completed"||!fact.success||!fact.code.empty()||fact.lessons.size()!=4)continue;
        if(std::find(fact.terms.participants.begin(),fact.terms.participants.end(),actor)==fact.terms.participants.end())continue;
        if(fact.id.empty()||std::count_if(military.training_facts.begin(),military.training_facts.end(),[&](const auto& other){return other.id==fact.id;})!=1)return fail("InvalidInsightTrainingSource",fact.id);
        if(fact.start_tick<0||fact.completed_tick<fact.start_tick||fact.completed_tick>common.identity.tick||
           fact.terms.garrison_id.empty()||fact.terms.place_id.empty()||fact.terms.unit_id.empty())
            return fail("InvalidInsightTrainingSource",fact.id);
        if(first&&(first->completed_tick<fact.completed_tick||
            (first->completed_tick==fact.completed_tick&&first->fact_id<fact.id)))continue;
        GrowthCampTrainingExperience source;
        source.fact_id=fact.id;source.actor_id=actor;source.garrison_id=fact.terms.garrison_id;
        source.place_id=fact.terms.place_id;source.unit_id=fact.terms.unit_id;
        source.world_id=common.identity.world_id;source.scenario_id=common.metadata.scenario_id;
        source.started_tick=fact.start_tick;source.completed_tick=fact.completed_tick;
        first=std::move(source);
    }
    return {true,std::move(first),{}};
}
// 公开本人权限保持原合同，不因新增内部reader暴露其他人的军务经历。
Outcome<std::optional<GrowthCampTrainingExperience>> first_growth_camp_training_experience(const CommonState& c,const MilitaryState& s,const StableId& actor){if(actor!=c.metadata.player_actor_id)return {false,{},common_failure("NotPlayerActor",actor).error};return read_verified_growth_camp_training_experience(c,s,actor);}
}
