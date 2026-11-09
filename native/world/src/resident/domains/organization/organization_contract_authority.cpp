// 组织采购的委派与收款账户沿现有任职检查，单一grant同时覆盖执行方式、金额、作用域和资源。
#include "organization_contract_authority.hpp"
#include "office_authority.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
// CommonStore生成的实例序号提供同tick内的真实先后；非法或非规范ID不得用于倒推任职。
std::optional<Integer> instance_sequence(const StableId& id,const StableId& prefix){
    const auto start=prefix+":";if(id.rfind(start,0)!=0)return {};const auto suffix=id.substr(start.size());Integer value=0;auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=suffix.data()+suffix.size()||value<1||std::to_string(value)!=suffix)return {};return value;
}
// 完整授权必须来自同一不可变grant；历史核验使用接受时点而不是今天的有效期。
bool covers_contract(const Grant& grant,const ActingAuthority& acting,const StableId& executor,const ScopeRef& scope,Integer amount,const StableId& payee,Integer tick){
    return grant.office_id==acting.office_id&&amount>0&&amount<=grant.amount_limit&&tick>=grant.valid_from&&(!grant.valid_until||tick<=*grant.valid_until)
        &&(executor==acting.actor_id||grant.may_delegate)
        &&std::find(grant.allowed_command_kinds.begin(),grant.allowed_command_kinds.end(),"claim_need")!=grant.allowed_command_kinds.end()
        &&std::find(grant.wallet_ids.begin(),grant.wallet_ids.end(),payee)!=grant.wallet_ids.end()
        &&std::any_of(grant.scopes.begin(),grant.scopes.end(),[&](const ScopeRef& allowed){return allowed.kind==scope.kind&&allowed.id==scope.id;});
}
// 任职epoch由原始任职及真实失效/继任事实顺序恢复；后续换任不会撤销旧合同的合法签发者。
Status historical_signer(const OrganizationState& state,const OfficeState& initial,const OfficeState& current,const ActingAuthority& acting,Integer accepted_tick,Integer accepted_sequence){
    Integer epoch=initial.epoch,previous_tick=0,previous_sequence=0,begin_tick=0;std::optional<Integer> end_tick;auto holder=initial.holder_actor_id;std::optional<StableId> accepted_holder;bool found=epoch==acting.authority_epoch;
    if(found)accepted_holder=holder;
    for(const auto& row:state.facts.rows()){
        const auto& fact=row.second;if(fact.kind!="organization_office_changed"||fact.office_id!=initial.id)continue;
        const auto sequence=instance_sequence(fact.id,"organization");
        if(!sequence||*sequence<=previous_sequence||fact.organization_id!=initial.organization_id||fact.tick<previous_tick||epoch==std::numeric_limits<Integer>::max())return common_failure("InvalidHistoricalContractAuthority",acting.office_id);
        if(epoch==acting.authority_epoch){if(*sequence<=accepted_sequence)return common_failure("InvalidHistoricalContractAuthority",acting.office_id);end_tick=fact.tick;}
        ++epoch;previous_tick=fact.tick;previous_sequence=*sequence;holder=fact.actor_id.empty()?std::nullopt:std::optional<StableId>{fact.actor_id};
        if(epoch==acting.authority_epoch){if(*sequence>=accepted_sequence)return common_failure("InvalidHistoricalContractAuthority",acting.office_id);found=true;accepted_holder=holder;begin_tick=fact.tick;}
    }
    if(epoch!=current.epoch||holder!=current.holder_actor_id||!found||accepted_holder!=std::optional<StableId>{acting.actor_id}||accepted_tick<begin_tick||(end_tick&&accepted_tick>*end_tick))return common_failure("InvalidHistoricalContractAuthority",acting.office_id);
    return common_success();
}
}
Outcome<AuthorityProof> check_contract_assignment_authority(
    const CommonStore &common, const OrganizationState &state,
    const OrganizationCatalog &catalog, const ActingAuthority &acting,
    const StableId &executor, const ScopeRef &scope, Integer amount,
    const StableId &payee_wallet_id, const OrganizationAuthorityClock *clock) {
  // This legacy TaskContract stores ActingAuthority only, not a historical
  // Grant/Basis. Preserve its original unsupported runtime path before writes.
  if (catalog.authority_lifecycle_enabled) {
    auto error = common_failure("NOT_IMPLEMENTED", "organization_accept_offer",
                                ErrorKind::unsupported_capability)
                     .error;
    error.reason = "OrganizationContractHistoricalAuthorityBasisRequired";
    return {false, {}, std::move(error)};
  }
  if (amount <= 0)
    return {
        false, {}, common_failure("OrganizationContractAuthorityDenied").error};
  const auto *wallet = common.wallet(payee_wallet_id);
  if (!wallet || wallet->owner.kind != Owner::Kind::organization ||
      wallet->owner.id != acting.organization_id)
    return {
        false, {}, common_failure("OrganizationContractPayeeRequired").error};
  SingleGrantAuthorityRequest request;
  request.acting = acting;
  request.command_kind = "claim_need";
  request.scope = scope;
  request.amount = amount;
  request.resources.push_back({ResourceKey::Kind::wallet, payee_wallet_id});
  request.requires_delegation = executor != acting.actor_id;
  auto original = check_current_grant_authority(
      common, state, catalog, catalog.authority_catalog_identity, request,
      clock);
  if (!original.ok)
    return {false, {}, original.error};
  return {true, std::move(original.value.authority), {}};
}
Status validate_organization_contract_authority(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const AffairsState& affairs,const TaskState& tasks){
    for(const auto& row:tasks.contracts.rows()){
        const auto& contract=row.second;if(contract.assignee.kind!=SubjectKind::organization)continue;
        if(!contract.accepted_authority||!contract.executor_actor_id||!contract.current_task_id)return common_failure("InvalidHistoricalContractAuthority",contract.id);
        const auto& acting=*contract.accepted_authority;const auto& terms=contract.frozen_terms;
        const auto* initial=catalog.offices.find(acting.office_id);const auto* current=state.offices.find(acting.office_id);const auto* claim=affairs.claims.find(contract.source_binding.claim_id);const auto* task=tasks.tasks.find(*contract.current_task_id);const auto* offer=task?tasks.offer_instances.find(task->offer_id):nullptr;const auto* payee=common.wallet(terms.payee_wallet_id);
        if(!initial||!current||!claim||!offer||!payee||!common.actor(acting.actor_id)||acting.organization_id!=contract.assignee.id||initial->organization_id!=acting.organization_id||current->organization_id!=acting.organization_id||claim->contract_id!=contract.id||claim->accepted_amount!=terms.agreed_quantity||claim->accepted_tick<0||claim->accepted_tick>common.tick()||payee->owner.kind!=Owner::Kind::organization||payee->owner.id!=acting.organization_id)return common_failure("InvalidHistoricalContractAuthority",contract.id);
        // 先减再比较避免溢出；任务完成时刻会变化，NeedClaim接受时刻才是授权起点。
        if(terms.definition.accept_ticks<=0||terms.definition.deadline_ticks<=0||terms.deadline_tick<terms.definition.deadline_ticks||terms.deadline_tick-terms.definition.deadline_ticks<terms.definition.accept_ticks||terms.deadline_tick-terms.definition.deadline_ticks-terms.definition.accept_ticks!=claim->accepted_tick)return common_failure("InvalidContractAcceptanceBoundary",contract.id);
        const auto acceptance_sequence=instance_sequence(claim->id,"claim");if(!acceptance_sequence||*acceptance_sequence>=common.state().next_instance_id)return common_failure("InvalidContractAcceptanceBoundary",contract.id);
        auto signer=historical_signer(state,*initial,*current,acting,claim->accepted_tick,*acceptance_sequence);if(!signer.ok)return signer;
        bool authorized=false;
        for(const auto& grant_id:initial->grant_ids){const auto* grant=catalog.grants.find(grant_id);if(grant&&covers_contract(*grant,acting,*contract.executor_actor_id,offer->accept_scope,terms.agreed_quantity,terms.payee_wallet_id,claim->accepted_tick)){authorized=true;break;}}
        if(!authorized)return common_failure("HistoricalContractGrantMismatch",contract.id);
    }
    return common_success();
}
} // 缺委派许可或收款资源授权明确拒绝，组织承诺不把酬金转为执行者私产。
