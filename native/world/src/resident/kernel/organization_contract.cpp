#include "native_organization_runtime.hpp"
// 组织接单桥只组合授权、真实接洽与既有任务托管；不另建组织钱包或替代个人履约。
#include "organization_contract.hpp"
#include "world_bundle.hpp"
#include "political_takeover_execution_world.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
Outcome<TaskQuote> preview_organization_contract(const WorldBundle& world,const StableId& issuer,const OrganizationContractCommand& command){
    if(!world.dynamic_enabled())return {false,{},common_failure("DynamicProfileRequired").error};
    if(issuer!=command.authority.actor_id)return {false,{},common_failure("CommandActorMismatch").error};
    auto issuer_guard=command_common_guard(world,issuer,"organization_accept_offer");if(!issuer_guard.ok)return {false,{},issuer_guard.error};
    const auto& task_command=command.task;const auto ref=task_command_offer(task_command);
    if(task_command.kind!="accept_offer"||ref.kind!=OfferRef::Kind::runtime||(!task_command.offer_id.empty()&&task_command.offer_id!=ref.id))return {false,{},common_failure("InvalidOrganizationContractCommand").error};
    if(!task_command.need_revision||!task_command.quantity)return {false,{},common_failure("MissingNeedTerms").error};
    auto executor_guard=command_common_guard(world,command.executor_actor_id,"accept_offer");if(!executor_guard.ok)return {false,{},executor_guard.error};
    auto organization=world.services.organization_access();
    if(!organization.actor_accepts_duty(command.executor_actor_id,command.authority.organization_id))return {false,{},common_failure("ContractAuthorityRequired").error};
    const auto* offer=world.tasks.offer_instances.find(ref.id);const auto* definition=resolve_offer_definition(world.tasks,*world.catalog->tasks,ref);
    if(!offer||!definition)return {false,{},common_failure("UnknownOffer").error};
    auto known=known_subject_view(world.common,world.social,organization,issuer,&world.interaction);const auto* need=world.affairs.needs.find(offer->source_need_id);
    const auto knows=[&](const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();};
    if(!need||(!knows(known.known_offer_ids,offer->id)&&!knows(known.known_need_ids,need->id)&&!knows(known.known_source_keys,need->source_key)))return {false,{},common_failure("SourceUnknown").error};
    if(const auto* prior=task_for(world.tasks,command.executor_actor_id,offer->id))if(prior->contract_id){const auto* contract=world.tasks.contracts.find(*prior->contract_id);if(contract&&contract->status!=ContractStatus::settled)return {false,{},common_failure("OfferAlreadyAccepted").error};}
    auto access=world.services.interaction_access();auto contact=validate_interaction(world.common,world.interaction,*world.catalog->interaction,access,command.executor_actor_id,task_command.context,"accept_offer");
    if(!contact.ok)return {false,{},contact.error};
    if(contact.value.role_id!=definition->accept_role)return {false,{},common_failure("RoleNotAuthorized").error};
    if(offer->accept_scope.kind!=ScopeRef::Kind::site||contact.value.place_id!=offer->accept_scope.id)return {false,{},common_failure("WrongContractSite").error};
    if(definition->accept_ticks<=0)return {false,{},common_failure("InvalidActionPreparation").error};
    NativeOrganizationAuthorityClock authority_clock(world);
    auto authority = check_contract_assignment_authority(
        world.common, world.organization, *world.catalog->organization,
        command.authority, command.executor_actor_id, offer->accept_scope,
        *task_command.quantity, command.payee_wallet_id, &authority_clock);
    if (!authority.ok)
      return {false, {}, authority.error};
    auto supply=PoliticalTakeoverExecutionWorldAccess::create(world);if(!supply.ok)return {false,{},supply.error};
    auto claim=prepare_need_claim(world.common,world.affairs,world.tasks,offer->source_need_id,*task_command.quantity,{SubjectKind::organization,command.authority.organization_id},command.executor_actor_id,offer->id,*task_command.need_revision,&organization,&command.authority,command.payee_wallet_id,supply.value->issued_supply());
    if(!claim.ok)return {false,{},claim.error};
    TaskQuote quote;quote.duration=definition->accept_ticks;quote.context=contact.value;quote.runtime=RuntimeTaskQuote{claim.value.need_id,claim.value.need_revision,claim.value.amount,claim.value.terms,claim.value.resource_versions};return {true,std::move(quote),{}};
}
Outcome<StableId> start_organization_contract(WorldBundle& world,const StableId& issuer,const OrganizationContractCommand& command){
    auto prepared=preview_organization_contract(world,issuer,command);if(!prepared.ok)return {false,{},prepared.error};
    const auto& quote=prepared.value;const auto& runtime=*quote.runtime;auto organization=world.services.organization_access();
    NeedClaimQuote claim;claim.need_id=runtime.need_id;claim.need_revision=runtime.need_revision;claim.amount=runtime.quantity;claim.assignee={SubjectKind::organization,command.authority.organization_id};claim.executor_actor_id=command.executor_actor_id;claim.offer_id=task_command_offer(command.task).id;claim.authority=command.authority;claim.terms.payee_wallet_id=command.payee_wallet_id;
    auto supply=PoliticalTakeoverExecutionWorldAccess::create(world);if(!supply.ok)return {false,{},supply.error};
    auto accepted=claim_need(world.common,world.affairs,world.tasks,claim,&organization,supply.value->issued_supply());if(!accepted.ok)return {false,{},accepted.error};
    ActionCoreView action;action.kind="accept_offer";action.actor_id=command.executor_actor_id;action.start_tick=world.common.tick();action.remaining_ticks=quote.duration;action.participants={command.executor_actor_id,quote.context.provider_actor_id};
    auto id=world.common.next_id("action");if(!id.ok)return {false,{},id.error};action.id=id.value;
    ActionMutation inserted;inserted.kind=ActionMutation::Kind::insert;inserted.action=action;auto written=world.common.apply_action(inserted);if(!written.ok)return {false,{},written.error};
    for(const auto& actor:action.participants){ActorMutation occupied;occupied.actor_id=actor;occupied.action_id=action.id;written=world.common.apply_actor(occupied);if(!written.ok)return {false,{},written.error};}
    TaskAction params;params.offer_id=claim.offer_id;params.task_id=accepted.value.task_id;params.context=quote.context;params.source_kind=command.task.source_kind;
    if(!world.tasks.actions.insert(action.id,std::move(params)))return {false,{},common_failure("DuplicateTaskAction",action.id,ErrorKind::internal).error};
    return {true,action.id,{}};
}
} // 只有执行者和接洽人占用个人时间，行政发令本身不改变发令者的行动。
