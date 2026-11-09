// 成熟来源是唯一出生的原生日与实际时钟，不是重建人物或重新抽取禀赋。
#include "family_child_maturation.hpp"
#include "family_birth_transaction.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<StableId> record_child_maturation_in_candidate(CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& child){
    auto history=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!history.ok)return {false,{},history.error};
    for(const auto& row:state.maturations.rows())if(row.second.actor_id==child)return {true,row.first,{}};
    const auto source=std::find_if(state.births.rows().begin(),state.births.rows().end(),[&](const auto& row){return row.second.actor_id==child;});const auto* person=c.actor(child);
    if(source==state.births.rows().end()||!person||!person->alive||c.tick()-source->second.tick<18*720)return {false,{},common_failure("BiologicalMaturationNotDue",child).error};
    const bool free=access.maturation_unoccupied_before&&access.maturation_unoccupied_before(child,c.tick(),{}).ok&&person->action_id.empty();
    auto id=c.next_id("family_birth");if(!id.ok)return id;
    ChildMaturationRecord record;record.actor_id=child;record.birth_fact_id=source->first;record.tick=c.tick();record.revision=c.revision();record.availability_enabled=free;
    if(!state.maturations.insert(id.value,record))return {false,{},common_failure("DuplicateChildMaturation",child).error};auto valid=c.append_fact({id.value,record.kind,"family_birth"});if(!valid.ok)return {false,{},valid.error};
    if(free){ActorMutation change;change.actor_id=child;change.available=true;valid=c.apply_actor(change);if(!valid.ok)return {false,{},valid.error};}
    auto after=validate_birth_arrangements(c,interactions,family,catalog,state,access);return after.ok?Outcome<StableId>{true,id.value,{}}:Outcome<StableId>{false,{},after.error};
}
Outcome<StableId> record_child_maturation(CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& child){return birth_detail::with_savepoint(c,state,[&]{return record_child_maturation_in_candidate(c,interactions,family,catalog,state,access,child);});}
}
