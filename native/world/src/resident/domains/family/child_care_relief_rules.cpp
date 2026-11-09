// 双方源及原责任先读，真实接受头先分配，再以同候选Common预约自己的粮；后验失败整体回滚。
#include "child_care_relief_resources.hpp"
#include "child_care_relief_transaction.hpp"
namespace hanmo::resident {
Outcome<StableId> accept_child_care_relief_in_candidate(CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,ChildCareReliefState& state,const ChildCareReliefAccess& access,const ChildCareReliefQuote& quote){
    auto bad=[](const Status& s)->Outcome<StableId>{return {false,{},s.error};};
    auto history=validate_child_care_relief(c,interactions,catalog,births,life,state,access);if(!history.ok)return {false,{},history.error};
    if(!access.player_before||!access.decision_maker_before)return bad(common_failure("ChildCareReliefDecisionHistoryRequired"));
    auto valid=revalidate_child_care_relief_source(c,meeting,interactions,contact,catalog,births,life,quote);if(!valid.ok)return bad(valid);
    if(history.value.active_by_child.find(quote.source.child_actor_id))return bad(common_failure("ChildCareReliefAlreadyActive"));
    for(const auto* person:{&quote.source.parent_actor_id,&quote.source.provider_actor_id}){valid=access.decision_maker_before(*person,c.tick(),{});if(!valid.ok)return bad(valid);}
    auto supply=preview_child_care_relief_supply(c,quote.source);if(!supply.ok)return {false,{},supply.error};
    auto id=c.next_id("family_care_relief");if(!id.ok)return id;ChildCareReliefAgreement agreement;agreement.source=quote.source;agreement.reserved_food=supply.value.quantity;
    if(!state.agreements.insert(id.value,std::move(agreement)))return bad(common_failure("DuplicateChildCareReliefAgreement"));
    valid=c.append_fact({id.value,"family_care_relief_accepted","family_care_relief"});if(!valid.ok)return bad(valid);
    auto recipe=reserve_child_care_relief_recipe(c,state,supply.value,id.value);if(!recipe.ok)return {false,{},recipe.error};valid=c.apply_resources(recipe.value);if(!valid.ok)return bad(valid);
    auto checked=validate_child_care_relief(c,interactions,catalog,births,life,state,access);return checked.ok?std::move(id):Outcome<StableId>{false,{},checked.error};
}
Outcome<StableId> withdraw_child_care_relief_in_candidate(CommonStore& c,const InteractionCatalog& interactions,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,ChildCareReliefState& state,const ChildCareReliefAccess& access,const StableId& actor,const StableId& accepted){
    auto bad=[](const Status& s)->Outcome<StableId>{return {false,{},s.error};};auto history=validate_child_care_relief(c,interactions,catalog,births,life,state,access);if(!history.ok)return {false,{},history.error};
    const auto* agreement=state.agreements.find(accepted);if(!agreement||history.value.closed_agreements.count(accepted))return bad(common_failure("ChildCareReliefActiveAgreementRequired"));const auto& source=agreement->source;
    if(actor!=c.state().metadata.player_actor_id||(actor!=source.provider_actor_id&&actor!=source.parent_actor_id))return bad(common_failure("ChildCareReliefPersonalDecisionRequired"));
    if(history.value.pending_by_agreement.find(accepted))return bad(common_failure("ChildCareReliefActionPending"));if(!access.decision_maker_before)return bad(common_failure("ChildCareReliefDecisionHistoryRequired"));auto valid=access.decision_maker_before(actor,c.tick(),{});if(!valid.ok)return bad(valid);
    const auto remaining=*history.value.remaining_food.find(accepted);auto recipe=release_child_care_relief_recipe(c,state,accepted,remaining);if(!recipe.ok)return {false,{},recipe.error};
    auto id=c.next_id("family_care_relief");if(!id.ok)return id;valid=c.apply_resources(recipe.value);if(!valid.ok)return bad(valid);
    ChildCareReliefClosure closed;closed.accepted_id=accepted;closed.actor_id=actor;closed.reason=actor==source.provider_actor_id?"provider_withdrew":"parent_withdrew";closed.tick=c.tick();closed.revision=c.revision();closed.released_food=remaining;
    if(!state.closures.insert(id.value,std::move(closed)))return bad(common_failure("DuplicateChildCareReliefClosure"));valid=c.append_fact({id.value,"family_care_relief_closed","family_care_relief"});if(!valid.ok)return bad(valid);
    auto checked=validate_child_care_relief(c,interactions,catalog,births,life,state,access);return checked.ok?std::move(id):Outcome<StableId>{false,{},checked.error};
}
Outcome<StableId> accept_child_care_relief(CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,ChildCareReliefState& state,const ChildCareReliefAccess& access,const ChildCareReliefQuote& quote){return care_relief_detail::with_savepoint(c,state,[&]{return accept_child_care_relief_in_candidate(c,meeting,interactions,contact,catalog,births,life,state,access,quote);});}
Outcome<StableId> withdraw_child_care_relief(CommonStore& c,const InteractionCatalog& interactions,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,ChildCareReliefState& state,const ChildCareReliefAccess& access,const StableId& actor,const StableId& accepted){return care_relief_detail::with_savepoint(c,state,[&]{return withdraw_child_care_relief_in_candidate(c,interactions,catalog,births,life,state,access,actor,accepted);});}
}
