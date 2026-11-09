// 死者不补签撤回同意；系统只消费先发生的真实死亡登记，释放尚未形成儿童的有限预约。
#include "family_birth_lifecycle.hpp"
#include "family_birth_transaction.hpp"
#include "actor_death_read.hpp"
namespace hanmo::resident {
Outcome<StableId> close_unborn_arrangement_after_death_in_candidate(CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& accepted_id){
    if(catalog.selected_version!=1)return {false,{},common_failure("BirthDueAtActualLocationWriterRequired",{},ErrorKind::unsupported_capability).error};
    auto history=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!history.ok)return {false,{},history.error};
    for(const auto& row:state.closures.rows())if(row.second.accepted_id==accepted_id)return {true,row.first,{}};
    if(history.value.born_acceptances.count(accepted_id))return {false,{},common_failure("BornChildCareResponsibilityRemains").error};
    const auto* accepted=state.replies.find(accepted_id);if(!accepted||!accepted->confirmation||accepted->decision!="accepted")return {false,{},common_failure("BirthAcceptedArrangementRequired").error};
    if(!access.life)return {false,{},common_failure("BirthParentDeathSourceRequired").error};
    const auto source=*accepted->confirmation;std::optional<ActorDeathRead> death;
    for(const auto* parent:{&source.speaker_actor_id,&source.listener_actor_id}){auto result=read_actor_death(c,*access.life,*parent);if(!result.ok)return {false,{},result.error};if(result.value){death=std::move(*result.value);break;}}
    if(!death||death->death_tick<accepted->tick)return {false,{},common_failure("BirthParentDeathSourceRequired").error};
    const auto* remaining=history.value.remaining_food.find(accepted_id);if(!remaining)return {false,{},common_failure("BirthCareReservationOwnerRequired").error};
    auto recipe=release_birth_care_recipe(c,source.terms,accepted_id,*remaining);if(!recipe.ok)return {false,{},recipe.error};
    auto id=c.next_id("family_birth");if(!id.ok)return id;
    BirthClosure closure;closure.accepted_id=accepted_id;closure.actor_id=death->actor_id;closure.reason="parent_death";closure.source_fact_id=death->death_fact_id;closure.tick=c.tick();closure.revision=c.revision();closure.released_food=*remaining;
    auto valid=c.apply_resources(recipe.value);if(!valid.ok)return {false,{},valid.error};
    if(!state.closures.insert(id.value,closure))return {false,{},common_failure("DuplicateBirthClosure").error};
    valid=c.append_fact({id.value,closure.kind,"family_birth"});if(!valid.ok)return {false,{},valid.error};
    auto after=validate_birth_arrangements(c,interactions,family,catalog,state,access);return after.ok?Outcome<StableId>{true,id.value,{}}:Outcome<StableId>{false,{},after.error};
}
Outcome<StableId> close_unborn_arrangement_after_death(CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& accepted){return birth_detail::with_savepoint(c,state,[&]{return close_unborn_arrangement_after_death_in_candidate(c,interactions,family,catalog,state,access,accepted);});}
}
