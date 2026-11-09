#include "resident/domains/organization/office_authority.hpp"
// 先保原行动与已受领责任；具名本人到柜台才核真实批准，最终仍由原命令重报价并提交。
#include "political_shop_registry_director.hpp"
#include "resident/domains/politics/licensing/political_shop_registry_detail.hpp"
#include "resident/domains/director/director_livelihood.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {namespace {
bool own_mandate(const PoliticalShopRegistryContext& x,const PoliticalShopRegistryEndpoint& endpoint,const ActingAuthority& authority){
 const auto& l=x.legal;const auto* definition=l.organization_catalog.offices.find(endpoint.office_id);const auto* office=l.organizations.offices.find(endpoint.office_id);if(!definition||!office||office->holder_actor_id!=std::optional<StableId>{authority.actor_id}||office->organization_id!=endpoint.organization_id)return false;
 OrganizationAccess access(l.common,l.organizations,l.organization_catalog,l.economy);if(!access.actor_accepts_duty(authority.actor_id,authority.organization_id))return false;
 for (const auto &id : office->grant_ids) {
   const auto *grant = l.organization_catalog.grants.find(id);
   if (!grant || !shop_law_detail::has(definition->grant_ids, id) ||
       grant->office_id != endpoint.office_id || !grant->wallet_ids.empty() ||
       !grant->inventory_ids.empty() ||
       !shop_law_detail::has(grant->allowed_command_kinds,
                             political_shop_registry_accept_command))
     continue;
   if (check_authority_with_single_grant(
           l.common, l.organizations, l.organization_catalog, authority,
           political_shop_registry_accept_command,
           {ScopeRef::Kind::site, endpoint.site_id}, 0, *grant, nullptr)
           .ok)
     return true;
 }
 return false;
}
}
Outcome<std::optional<DecisionProposal>> propose_director_shop_registry_acceptance(
 const PoliticalShopRegistryContext& x,const PoliticalTakeoverRead& verified,const TravelCatalog& travel,const KnownSubjectView& known,const DirectorPolicy& policy,const PoliticalShopRegistryIntent& intent,const DecisionProposal& base,Integer limit){
 const auto& l=x.legal;if(!base.transition||base.subject_id!=known.actor_id||known.subject.kind!=SubjectKind::actor||known.subject.id!=known.actor_id||known.tick!=l.common.tick()||known.revision!=l.common.revision()||base.transition->expected_revision!=known.revision||limit<1||limit>64||l.phase||x.control.phase)return {false,{},common_failure("InvalidShopRegistryDirectorProposal",known.actor_id,ErrorKind::internal).error};
 const auto* person=l.common.actor(known.actor_id);if(!person||!std::holds_alternative<SettlementLocation>(person->location))return {true,std::nullopt,{}};
 if(base.command||base.transition->current_action_id||!known.commitments.empty()||!base.transition->commitment_ids.empty()||base.transition->template_id=="maintain_commitment"||known.actor_id==l.common.state().metadata.player_actor_id||!known.alive||!known.available||!known.current_action_id.empty()||known.interaction_busy)return {true,std::nullopt,{}};
 if(intent.actor_id!=known.actor_id)return {true,std::nullopt,{}};const auto* selected=x.catalog.policies.find(intent.registry_policy_id);if(!selected||selected->organization_id!=policy.organization_id)return {false,{},common_failure("PoliticalShopRegistryIntentSourceMismatch",intent.id).error};auto endpoint=shop_registry_detail::endpoint(x,*selected);if(!endpoint.ok)return {false,{},endpoint.error};
 const auto* office=l.organizations.offices.find(endpoint.value.office_id);if(!office||office->holder_actor_id!=std::optional<StableId>{known.actor_id})return {true,std::nullopt,{}};ActingAuthority authority{office->organization_id,office->id,office->epoch,known.actor_id};if(!own_mandate(x,endpoint.value,authority))return {true,std::nullopt,{}};
 auto out=base;auto next=std::make_shared<DirectorTransition>(*base.transition);out.transition=next;next->template_id=political_shop_registry_intent_template;next->goal_ref=intent.id;next->current_action_id.reset();next->next_step_index=0;next->wait_reason.clear();next->livelihood.reset();next->pending_life_action_id.reset();next->last_life_fact_id.reset();out.reason_fact_id.clear();
 if(known.place_id!=endpoint.value.site_id){out.command=director_move_toward(travel,known,endpoint.value.site_id);next->phase=out.command?PlanPhase::delivering:PlanPhase::waiting;next->wait_reason=out.command?StableId{}:"PoliticalShopRegistryRouteUnavailable";return {true,std::move(out),{}};}
 auto valid=validate_political_shop_registry_successions(x);if(!valid.ok)return {false,{},valid.error};const auto* court=l.catalog.courts.find(selected->court_id);if(!court)return {false,{},common_failure("PoliticalShopRegistryCourtUnknown",selected->court_id).error};
 auto control=read_political_shop_registry_control(x.control,l.politics,l.politics_catalog,*court);if(!control.ok)return {false,{},control.error};next->phase=PlanPhase::waiting;
 if(control.value.revision<1||control.value.organization_id!=endpoint.value.organization_id){next->wait_reason="PoliticalShopRegistryAwaitingControl";return {true,std::move(out),{}};}
 next->goal_ref=control.value.registration_fact_id;out.reason_fact_id=control.value.registration_fact_id;if(control.value.approval_fact_id.empty()){next->wait_reason="PoliticalShopRegistryAwaitingApproval";return {true,std::move(out),{}};}
 const auto* previous=shop_registry_detail::latest(x,court->id,l.common.state().next_instance_id);if(previous&&previous->registration_fact_id==control.value.registration_fact_id)return {true,std::nullopt,{}};
 PoliticalAcceptShopRegistry command{authority,selected->id,control.value.registration_fact_id,control.value.approval_fact_id,previous?std::optional<StableId>{previous->id}:std::nullopt};auto quote=preview_political_shop_registry_acceptance(x,verified,known.actor_id,command);if(!quote.ok){if(quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};next->wait_reason=quote.error.original.code;return {true,std::move(out),{}};}
 out.command=std::make_shared<const CommandPayload>(CommandPayload{political_shop_registry_accept_command,std::move(command)});next->phase=PlanPhase::completed;return {true,std::move(out),{}};
}
}
