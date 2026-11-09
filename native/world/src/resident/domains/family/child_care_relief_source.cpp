#include "resident/kernel/catalog_section_observation.hpp"
// 本人节点只对本次实际出生对象授权；私粮仍须后续受检Common recipe，任何全局扫描不授知情或监护。
#include "actor_life_eligibility.hpp"
#include "child_care_relief_source.hpp"
#include "actor_age_birth_read.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
bool reference(const StableId& s){return !s.empty()&&s.size()<=256;}
const DialogueDefinition* dialogue(const InteractionCatalog& c,const StableId& id){for(const auto& d:c.dialogues)if(d.id==id)return &d;return nullptr;}
const DialogueNode* node(const DialogueDefinition& d,const StableId& id){for(const auto& n:d.nodes)if(n.id==id)return &n;return nullptr;}
bool context_equal(const InteractionContext& a,const InteractionContext& b){return std::tie(a.place_id,a.provider_actor_id,a.role_id,a.kind,a.encounter_id,a.input_error)==std::tie(b.place_id,b.provider_actor_id,b.role_id,b.kind,b.encounter_id,b.input_error);}
Status shape(const ChildCareReliefTerms& t){
    if(!reference(t.place_id)||!reference(t.source_inventory_id)||!reference(t.food_item_id)||t.period_ticks<1||t.period_ticks>720||t.food_per_period<1||t.food_per_period>10000||t.prepaid_periods<1||t.prepaid_periods>360)return common_failure("InvalidChildCareReliefTerms");
    return common_success();
}
// 原照护的实际中断才可接手；正常尚未开工不能凭空作退出。已完成有限计划可另获有限帮助。
Outcome<std::pair<StableId,StableId>> stop_source(const CommonStore& c,const FamilyBirthState& births,const ChildCareOriginRead& origin){
    if(!origin.pending_action_id.empty())return {false,{},common_failure("ChildCareOriginalActionPending").error};
    if(!origin.child_death_fact_id.empty())return {false,{},common_failure("ChildCareRecipientDied").error};
    if(origin.original_schedule_closed){
        StableId id;for(const auto& row:births.closures.rows())if(row.second.accepted_id==origin.accepted_fact_id){if(!id.empty())return {false,{},common_failure("ChildCareOriginClosureMismatch").error};id=row.first;}
        if(id.empty())return {false,{},common_failure("ChildCareOriginClosureMismatch").error};
        return {true,{"prepaid_care_completed",id},{}};
    }
    if(!origin.caregiver_death_fact_id.empty())return {true,{"caregiver_died",origin.caregiver_death_fact_id},{}};
    if(origin.last_care_code=="CaregiverStopped"&&!origin.last_care_fact_id.empty())return {true,{"caregiver_stopped",origin.last_care_fact_id},{}};
    return {false,{},common_failure("ChildCareReliefOriginalStopRequired").error};
}
// 原父母确实同场参与该次出生；这里只借其既有责任，不为不相干人物生成全知或法定代表权。
Status parent_source(const ChildCareOriginRead& origin,const StableId& parent,Integer tick){
    if(std::find(origin.biological_parent_ids.begin(),origin.biological_parent_ids.end(),parent)==origin.biological_parent_ids.end()||std::find(origin.living_parent_ids.begin(),origin.living_parent_ids.end(),parent)==origin.living_parent_ids.end())return common_failure("ChildCareReliefLivingParentRequired");
    if(tick<origin.birth_tick||tick-origin.birth_tick>=18*720)return common_failure("ChildCareReliefMinorRequired");
    return common_success();
}
// 海上旧锚点不代表儿童今天在室内，原私库存以实际Owner和本人背包双重核对。
Status physically_present(const CommonStore& c,const StableId& person,const StableId& place,bool adult){
    const auto* actor=c.actor(person);if(!actor||!actor->alive||(adult&&!actor->available)||!actor->action_id.empty())return common_failure("ChildCareReliefParticipantUnavailable",person);
    auto location=c.resolved_location(person);if(!location.ok)return {false,{},location.error};const auto* at=std::get_if<SettlementLocation>(&location.value);
    return at&&at->place_id==place?common_success():common_failure("ChildCareReliefNotAtPlace",person);
}
Status source_shape(const CommonStore& c,const InteractionCatalog& interactions,const ChildCareReliefCatalog& catalog,const ChildCareReliefSource& source){
    if(!interactions.common)return common_failure("ChildCareReliefCommonRequired");auto valid=validate_child_care_relief_catalog(catalog,*interactions.common,interactions);if(!valid.ok)return valid;
    const auto* offer=catalog.offers.find(source.offer_id);if(!catalog.enabled||!offer||source.provider_actor_id!=offer->provider_actor_id||source.dialogue_id!=offer->dialogue_id||source.node_id!=offer->node_id||source.terms_version!=offer->terms_version||!child_care_relief_terms_equal(source.terms,offer->terms))return common_failure("ChildCareReliefOfferMismatch");
    for(const auto* id:{&source.parent_actor_id,&source.child_actor_id,&source.birth_fact_id,&source.original_accepted_id,&source.original_stop_fact_id,&source.session_id,&source.scenario_id})if(!reference(*id))return common_failure("InvalidChildCareReliefReference");
    if(source.parent_actor_id==source.provider_actor_id||source.child_actor_id==source.parent_actor_id||source.child_actor_id==source.provider_actor_id||source.world_id!=c.state().identity.world_id||std::all_of(source.world_id.begin(),source.world_id.end(),[](auto x){return x==0;})||source.scenario_id!=c.state().metadata.scenario_id||source.tick<0||source.tick>c.tick()||source.revision<0||source.revision>c.revision())return common_failure("ChildCareReliefWorldTimeMismatch");
    const auto* talk=dialogue(interactions,source.dialogue_id);const auto* saying=node(*talk,source.node_id);
    if(source.node_kind!=saying->kind||!face_to_face_contact_context_valid(source.context)||source.context.kind||source.context.provider_actor_id!=source.provider_actor_id||source.context.place_id!=source.terms.place_id||source.context.role_id!=talk->context_role)return common_failure("ChildCareReliefContactMismatch");
    return common_success();
}
}
bool child_care_relief_terms_equal(const ChildCareReliefTerms& a,const ChildCareReliefTerms& b){return std::tie(a.place_id,a.source_inventory_id,a.food_item_id,a.period_ticks,a.food_per_period,a.prepaid_periods)==std::tie(b.place_id,b.source_inventory_id,b.food_item_id,b.period_ticks,b.food_per_period,b.prepaid_periods);}
bool child_care_relief_source_equal(const ChildCareReliefSource& a,const ChildCareReliefSource& b){return std::tie(a.offer_id,a.provider_actor_id,a.parent_actor_id,a.child_actor_id,a.birth_fact_id,a.original_accepted_id,a.original_stop_fact_id,a.original_stop_kind,a.dialogue_id,a.node_id,a.node_kind,a.session_id,a.scenario_id,a.world_id,a.terms_version,a.tick,a.revision,a.provider_birth_tick,a.parent_birth_tick)==std::tie(b.offer_id,b.provider_actor_id,b.parent_actor_id,b.child_actor_id,b.birth_fact_id,b.original_accepted_id,b.original_stop_fact_id,b.original_stop_kind,b.dialogue_id,b.node_id,b.node_kind,b.session_id,b.scenario_id,b.world_id,b.terms_version,b.tick,b.revision,b.provider_birth_tick,b.parent_birth_tick)&&context_equal(a.context,b.context)&&child_care_relief_terms_equal(a.terms,b.terms);}
Status validate_child_care_relief_catalog(const ChildCareReliefCatalog& catalog,const CommonCatalog& common,const InteractionCatalog& interactions){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 28, "ChildCareReliefCatalog", "validate_child_care_relief_catalog", &catalog);

    if(!catalog.enabled)return catalog.offers.empty()?common_success():common_failure("ChildCareReliefProfileRequired");
    if(catalog.offers.empty()||catalog.offers.size()>4096)return common_failure("InvalidChildCareReliefCatalog");std::set<std::tuple<StableId,StableId,StableId>> bound;
    for(const auto& row:catalog.offers.rows()){
        const auto& offer=row.second;for(const auto* id:{&offer.id,&offer.provider_actor_id,&offer.dialogue_id,&offer.node_id})if(!reference(*id))return common_failure("InvalidChildCareReliefOffer",row.first);
        if(row.first!=offer.id||offer.terms_version!=1||std::find(common.actor_ids.begin(),common.actor_ids.end(),offer.provider_actor_id)==common.actor_ids.end())return common_failure("UnknownChildCareReliefProvider",row.first);
        auto valid=shape(offer.terms);if(!valid.ok)return valid;const auto* talk=dialogue(interactions,offer.dialogue_id);const auto* saying=talk?node(*talk,offer.node_id):nullptr;const auto* role=talk?common.roles.find(talk->context_role):nullptr;
        if(!talk||!saying||(saying->kind!="say"&&saying->kind!="choice")||!role||role->place_id!=offer.terms.place_id||!common.places.find(offer.terms.place_id)||!common.items.find(offer.terms.food_item_id))return common_failure("ChildCareReliefNodeRequired",row.first);
        if(!bound.emplace(offer.provider_actor_id,offer.dialogue_id,offer.node_id).second)return common_failure("DuplicateChildCareReliefNode",row.first);
    }return common_success();
}
Outcome<ChildCareReliefQuote> preview_child_care_relief_source(const CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,const StableId& parent,const StableId& child,const StableId& offer_id){
    auto bad=[](const Status& s)->Outcome<ChildCareReliefQuote>{return {false,{},s.error};};
    if(parent!=c.state().metadata.player_actor_id||!interactions.common)return bad(common_failure("ChildCareReliefPersonalDecisionRequired"));
    auto valid=validate_child_care_relief_catalog(catalog,*interactions.common,interactions);if(!valid.ok)return bad(valid);const auto* offer=catalog.offers.find(offer_id);if(!catalog.enabled||!offer)return bad(common_failure("ChildCareReliefUnavailable"));
    auto origin=read_child_care_origin(c,births,life,child);if(!origin.ok)return {false,{},origin.error};valid=parent_source(origin.value,parent,c.tick());if(!valid.ok)return bad(valid);auto stopped=stop_source(c,births,origin.value);if(!stopped.ok)return {false,{},stopped.error};
    auto parent_age=read_actor_age(c,*interactions.common,births,parent),provider_age=read_actor_age(c,*interactions.common,births,offer->provider_actor_id);if(!parent_age.ok)return {false,{},parent_age.error};if(!provider_age.ok)return {false,{},provider_age.error};if(!parent_age.value.adult||!provider_age.value.adult)return bad(common_failure("ChildCareReliefAdultsRequired"));
    for(const auto* person:{&parent,&offer->provider_actor_id,&child}){valid=physically_present(c,*person,offer->terms.place_id,*person!=child);if(!valid.ok)return bad(valid);}
    const auto* provider=c.actor(offer->provider_actor_id);const auto* bag=c.inventory(offer->terms.source_inventory_id);const Owner owner{Owner::Kind::actor,offer->provider_actor_id,{}};
    if(!bag||bag->id!=provider->inventory_id||bag->owner.extra||!owner_equal(bag->owner,owner))return bad(common_failure("ChildCareReliefOwnInventoryRequired"));
    auto proof=preview_authored_node_source(c,meeting,interactions,contact,parent,offer->provider_actor_id);if(!proof.ok)return {false,{},proof.error};if(proof.value.dialogue_id!=offer->dialogue_id||proof.value.node_id!=offer->node_id)return bad(common_failure("ChildCareReliefNodeNotCurrent"));
    ChildCareReliefSource s;s.offer_id=offer_id;s.provider_actor_id=offer->provider_actor_id;s.parent_actor_id=parent;s.child_actor_id=child;s.birth_fact_id=origin.value.birth_fact_id;s.original_accepted_id=origin.value.accepted_fact_id;s.original_stop_kind=stopped.value.first;s.original_stop_fact_id=stopped.value.second;s.dialogue_id=offer->dialogue_id;s.node_id=offer->node_id;s.node_kind=proof.value.node_kind;s.session_id=proof.value.session_id;s.scenario_id=c.state().metadata.scenario_id;s.world_id=c.state().identity.world_id;s.context=proof.value.context;s.terms=offer->terms;s.tick=c.tick();s.revision=c.revision();s.provider_birth_tick=provider_age.value.birth_tick;s.parent_birth_tick=parent_age.value.birth_tick;
    valid=source_shape(c,interactions,catalog,s);return valid.ok?Outcome<ChildCareReliefQuote>{true,{proof.value,std::move(s)},{}}:bad(valid);
}
Status revalidate_child_care_relief_source(const CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,const ChildCareReliefQuote& quote){
    auto valid=revalidate_authored_node_source(c,meeting,interactions,contact,quote.current);if(!valid.ok)return valid;
    auto fresh=preview_child_care_relief_source(c,meeting,interactions,contact,catalog,births,life,quote.source.parent_actor_id,quote.source.child_actor_id,quote.source.offer_id);if(!fresh.ok)return {false,{},fresh.error};
    if(quote.current.actor_id!=quote.source.parent_actor_id||quote.current.provider_actor_id!=quote.source.provider_actor_id||!child_care_relief_source_equal(quote.source,fresh.value.source))return common_failure("StaleChildCareReliefSource");return common_success();
}
Status validate_child_care_relief_source_before_acceptance(const CommonStore& c,const InteractionCatalog& interactions,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,const ChildCareReliefSource& s,const StableId& accepted){
    auto valid=source_shape(c,interactions,catalog,s);if(!valid.ok)return valid;
    auto origin=read_child_care_origin_before_relief(c,births,life,s.child_actor_id,s.tick,accepted);if(!origin.ok)return {false,{},origin.error};valid=parent_source(origin.value,s.parent_actor_id,s.tick);if(!valid.ok)return valid;auto stopped=stop_source(c,births,origin.value);if(!stopped.ok)return {false,{},stopped.error};
    if(origin.value.birth_fact_id!=s.birth_fact_id||origin.value.accepted_fact_id!=s.original_accepted_id||stopped.value.first!=s.original_stop_kind||stopped.value.second!=s.original_stop_fact_id)return common_failure("ChildCareReliefOriginalSourceMismatch");
    auto parent=read_actor_age_before_fact(c,*interactions.common,births,s.parent_actor_id,s.tick,accepted),provider=read_actor_age_before_fact(c,*interactions.common,births,s.provider_actor_id,s.tick,accepted);if(!parent.ok)return {false,{},parent.error};if(!provider.ok)return {false,{},provider.error};
    if(!parent.value.adult||!provider.value.adult||parent.value.birth_tick!=s.parent_birth_tick||provider.value.birth_tick!=s.provider_birth_tick)return common_failure("ChildCareReliefAdultSourceMismatch");
    auto alive=read_actor_alive_before_fact(c,life,s.provider_actor_id,s.tick,accepted);if(!alive.ok)return {false,{},alive.error};if(!alive.value)return common_failure("ChildCareReliefProviderAlreadyDead");return common_success();
}
}
