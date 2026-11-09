// 不重复构建人物或重抽随机；原武艺初值读在此只复用同一身份/真实创建来源内核。
#include "growth_condition_origin.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
Outcome<GrowthPersonalConditionActorOrigin> read_growth_condition_actor_origin(const CommonStore& c,const GrowthSkillOriginContext& context,const GrowthPersonalMasteryInitialCatalog& initial,const StableId& actor){
    auto fail=[&](const StableId& code)->Outcome<GrowthPersonalConditionActorOrigin>{return {false,{},common_failure(code,actor).error};};
    if(initial.scenario_id.empty()||initial.scenario_id!=c.state().metadata.scenario_id)return fail("GrowthConditionInitialScenarioMismatch");
    const std::set<StableId> installed(context.common.actor_ids.begin(),context.common.actor_ids.end());if(installed.size()!=context.common.actor_ids.size()||initial.actors.size()!=installed.size())return fail("GrowthConditionInitialActorMismatch");for(const auto& id:installed)if(!initial.actors.find(id))return fail("GrowthConditionInitialActorMismatch");
    auto original=read_growth_skill_origin(c,context,actor,"martial");if(!original.ok)return {false,{},original.error};const auto& source=original.value;
    GrowthPersonalConditionActorOrigin out;out.source_identity=source.source_identity;out.actor_id=actor;out.created_tick=source.created_tick;out.hp=100;out.fatigue=0;
    if(source.origin_kind=="effective_authored_actor"){
        if(source.authored_actor_id!=actor||source.prototype_actor_id!=actor||!installed.count(actor)||!source.source_fact_ids.empty()||source.created_tick!=0)return fail("GrowthConditionInitialActorMismatch");out.kind=GrowthPersonalConditionOriginKind::installed_actor;out.authored_actor_id=actor;
    }else{
        if(installed.count(actor)||!source.authored_actor_id.empty()||source.source_fact_ids.size()!=2||source.source_fact_ids[1]!=actor)return fail("GrowthConditionCreationSourceMismatch");
        if(source.origin_kind=="character_creation")out.kind=GrowthPersonalConditionOriginKind::character_creation;
        else if(source.origin_kind=="adoption_creation")out.kind=GrowthPersonalConditionOriginKind::adoption;
        else if(source.origin_kind=="biological_birth")out.kind=GrowthPersonalConditionOriginKind::birth;
        else return fail("GrowthConditionCreationSourceMismatch");
        auto number=[](const StableId& id){const auto colon=id.rfind(':');if(colon==StableId::npos)return Integer{-1};const auto tail=id.substr(colon+1);Integer n=-1;auto r=std::from_chars(tail.data(),tail.data()+tail.size(),n);return r.ec==std::errc{}&&r.ptr==tail.data()+tail.size()&&n>0&&std::to_string(n)==tail?n:Integer{-1};};
        const auto allocated=number(actor),business=number(source.source_fact_ids[0]);if(business<1||allocated<=business||allocated>=c.state().next_instance_id)return fail("GrowthConditionCreationSourceOrder");
        out.creation_fact_id=actor;out.business_source_fact_id=source.source_fact_ids[0];
    }
    return {true,std::move(out),{}};
}
}
