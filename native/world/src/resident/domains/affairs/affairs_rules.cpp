// 需求刷新与份额裁决只消费真实来源；接单、资金和合同必须随同一个外层候选发布。
#include "affairs_model.hpp"
#include "affairs_policy_validation.hpp"
#include "affairs_supply_suspension.hpp"
#include "issued_supply/affairs_issued_supply.hpp"
#include "objective_result_consumer.hpp"
#include "quantity_contribution_time2.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_need_adapter.hpp"
#include "state_need_catalog.hpp"
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 所有数量乘加先检查整数边界，失败不能降格为截断数量。
bool add_ok(Integer a, Integer b, Integer &out) {
  if (a < 0 || b < 0 || a > std::numeric_limits<Integer>::max() - b)
    return false;
  out = a + b;
  return true;
}
bool mul_ok(Integer a, Integer b, Integer &out) {
  if (a < 0 || b < 0 || (b && a > std::numeric_limits<Integer>::max() / b))
    return false;
  out = a * b;
  return true;
}
bool terminal(NeedStatus status) {
  return status == NeedStatus::fulfilled || status == NeedStatus::expired ||
         status == NeedStatus::cancelled;
}
bool occupying(ClaimStatus status) {
  return status == ClaimStatus::accepted || status == ClaimStatus::in_progress;
}
bool same_subject(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
// 终结责任独立于份额释放；取消或过期的在途合同仍阻止事项关闭。
bool unsettled(const AffairsState &state, const TaskState &tasks,
               const Need &need) {
  for (const auto &id : need.claim_ids) {
    auto *claim = state.claims.find(id);
    auto *contract = claim ? tasks.contracts.find(claim->contract_id) : nullptr;
    if (!contract || contract->status != ContractStatus::settled)
      return true;
  }
  return false;
}
// 状态仅由当前缺口和已承诺责任推导，不把验收历史从实际库存重复扣除。
Status update_need(AffairsState &state, const TaskState &tasks, Need &need,
                   Integer tick) {
  const auto gap = need_gap(need);
  const bool pending = unsettled(state, tasks, need);
  if (need.status == NeedStatus::expired ||
      need.status == NeedStatus::cancelled)
    return common_success();
  if (need.deadline_tick && tick > *need.deadline_tick && gap > 0) {
    need.status = NeedStatus::expired;
    return common_success();
  }
  if (gap == 0) {
    need.status = pending ? NeedStatus::settling : NeedStatus::fulfilled;
    return common_success();
  }
  auto available = need_available_amount(state, need);
  if (!available.ok)
    return {false, {}, available.error};
  need.status = (available.value < gap || need.fulfilled_amount > 0)
                    ? NeedStatus::partial
                    : NeedStatus::open;
  return common_success();
}
// 作者映射必须明确单位、验收和受托经费；不支持的政策不能用默认分支掩盖。
Status policy_valid(const NeedPolicy &p) { return validate_need_policy(p); }
// 义务需求逐张欠据消费原经济事实；这里只登记责任，绝不支付、发奖励或产生可花余额。
Status reconcile_obligation_needs(CommonStore &common, AffairsState &state,
                                  const AffairsCatalog &catalog,
                                  const DynamicResultRead &results,
                                  Integer tick) {
  if (!catalog.obligation_needs)
    return common_success();
  if (!state.obligation_needs)
    return common_failure("AffairsObligationVersionMismatch");
  for (const auto &fact_id : results.fact_ids(-1, tick)) {
    const auto fact = results.fact(fact_id);
    if (!fact || !fact->obligation)
      continue;
    const auto &observation = *fact->obligation;
    const ObligationNeedPolicy *policy = nullptr;
    for (const auto &row : catalog.obligation_policies.rows())
      if (row.second.obligation_id == observation.obligation_id) {
        policy = &row.second;
        break;
      }
    if (!policy)
      continue;
    const auto *header = common.state().facts.find(fact_id);
    if (!header || header->domain != "local_economy" ||
        header->kind != fact->kind || fact->id != fact_id ||
        observation.source_fact_id != fact_id || fact->tick < 0 ||
        fact->tick > tick || observation.due_tick < 0 ||
        observation.due_tick > fact->tick ||
        observation.principal_id != policy->organization_id ||
        !common.actor(observation.beneficiary_id) ||
        observation.arrear_id.empty() || observation.due_amount < 0 ||
        observation.paid_amount < 0 ||
        observation.paid_amount > observation.due_amount ||
        observation.outstanding_amount !=
            observation.due_amount - observation.paid_amount)
      return common_failure("InvalidObligationNeedSource", fact_id);
    if (fact->kind != "wage_due" && fact->kind != "contract_payment_due" &&
        fact->kind != "obligation_payment")
      return common_failure("UnsupportedObligationNeedSource", fact_id,
                            ErrorKind::unsupported_capability);
    Need *need = nullptr;
    for (auto &row : state.needs.rows())
      if (row.second.measure == NeedMeasure::outstanding_obligation &&
          row.second.source_domain == "local_economy" &&
          row.second.obligation_id == observation.obligation_id &&
          row.second.arrear_id == observation.arrear_id) {
        need = &row.second;
        break;
      }
    if (need && std::find(need->receipt_ids.begin(), need->receipt_ids.end(),
                          fact_id) != need->receipt_ids.end())
      continue;
    if (!need) {
      auto affair_id = common.next_id("affair");
      if (!affair_id.ok)
        return {false, {}, affair_id.error};
      auto need_id = common.next_id("need");
      if (!need_id.ok)
        return {false, {}, need_id.error};
      WorldAffair affair;
      affair.id = affair_id.value;
      affair.semantic_key = policy->semantic_key + ":" + observation.arrear_id;
      affair.principal = {SubjectKind::organization, observation.principal_id};
      affair.opened_tick = tick;
      affair.cause_fact_ids = {fact_id};
      affair.need_ids = {need_id.value};
      state.affairs.insert(affair_id.value, std::move(affair));
      Need created;
      created.id = need_id.value;
      created.affair_id = affair_id.value;
      created.source_domain = "local_economy";
      created.source_key = "obligation:" + observation.obligation_id + ":" +
                           observation.arrear_id;
      created.beneficiary = {SubjectKind::actor, observation.beneficiary_id};
      created.unit = "money";
      created.measure = NeedMeasure::outstanding_obligation;
      created.created_tick = tick;
      created.policy_id = policy->id;
      created.obligation_id = observation.obligation_id;
      created.arrear_id = observation.arrear_id;
      created.target_amount = observation.due_amount;
      created.last_observed_tick = tick;
      state.needs.insert(need_id.value, std::move(created));
      need = state.needs.find(need_id.value);
    }
    if (need->policy_id != policy->id ||
        need->target_amount != observation.due_amount ||
        need->beneficiary.id != observation.beneficiary_id ||
        need->fulfilled_amount > observation.paid_amount ||
        need->revision == std::numeric_limits<Integer>::max() ||
        need->source_revision == std::numeric_limits<Integer>::max())
      return common_failure("ObligationNeedSourceMismatch", fact_id);
    need->fulfilled_amount = observation.paid_amount;
    need->source_fact_id = fact_id;
    need->last_observed_tick = tick;
    ++need->revision;
    ++need->source_revision;
    need->receipt_ids.push_back(fact_id);
    need->status = observation.outstanding_amount == 0
                       ? NeedStatus::fulfilled
                       : (observation.paid_amount > 0 ? NeedStatus::partial
                                                      : NeedStatus::open);
  }
  return common_success();
}
} // namespace
Integer need_gap(const Need &n) {
  return std::max<Integer>(0, n.target_amount -
                                  (n.measure == NeedMeasure::stock_gap
                                       ? n.physical_amount
                                       : n.fulfilled_amount));
}
Outcome<Integer> need_available_amount(const AffairsState &state,
                                       const Need &need) {
  Integer committed = 0;
  for (const auto &id : need.claim_ids) {
    auto *c = state.claims.find(id);
    if (!c || c->need_id != need.id || c->fulfilled_amount < 0 ||
        c->fulfilled_amount > c->accepted_amount)
      return {false, {}, common_failure("InvalidNeedClaim", id).error};
    if (occupying(c->status) &&
        !add_ok(committed, c->accepted_amount - c->fulfilled_amount, committed))
      return {false, {}, common_failure("NeedQuantityOverflow", need.id).error};
  }
  return {true, std::max<Integer>(0, need_gap(need) - committed), {}};
}
Outcome<AffairsState> make_initial_affairs(const AffairsCatalog &catalog) {
  if (catalog.state_need_policies) {
    auto valid = validate_state_need_catalog(*catalog.state_need_policies);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  std::set<StableId> sources, keys;
  for (const auto &row : catalog.policies.rows()) {
    auto r = policy_valid(row.second);
    if (!r.ok)
      return {false, {}, r.error};
    if (row.first != row.second.id ||
        !sources.insert(row.second.source_key).second ||
        !keys.insert(row.second.semantic_key).second)
      return {
          false, {}, common_failure("DuplicateNeedPolicy", row.first).error};
  }
  if (!catalog.obligation_needs && !catalog.obligation_policies.empty())
    return {
        false, {}, common_failure("ObligationNeedsCapabilityRequired").error};
  std::set<StableId> obligations;
  for (const auto &row : catalog.obligation_policies.rows()) {
    const auto &policy = row.second;
    if (row.first != policy.id || policy.id.empty() ||
        policy.obligation_id.empty() || policy.organization_id.empty() ||
        policy.semantic_key.empty() || catalog.policies.find(policy.id) ||
        !obligations.insert(policy.obligation_id).second ||
        !keys.insert(policy.semantic_key).second)
      return {false,
              {},
              common_failure("InvalidObligationNeedPolicy", row.first).error};
  }
  if (catalog.fiscal_need_policies &&
      catalog.fiscal_need_policies->version != 1)
    return {false, {}, common_failure("UnsupportedFiscalNeedVersion").error};
  if (catalog.fiscal_need_policies)
    for (const auto &row : catalog.fiscal_need_policies->policies.rows())
      if (catalog.policies.find(row.first) ||
          catalog.obligation_policies.find(row.first))
        return {false,
                {},
                common_failure("DuplicateFiscalNeedPolicy", row.first).error};
  if (catalog.cohort_wage_need_policies) {
    if (catalog.cohort_wage_need_policies->version != 1)
      return {
          false, {}, common_failure("UnsupportedCohortWageNeedVersion").error};
    for (const auto &row : catalog.cohort_wage_need_policies->policies.rows())
      if (catalog.policies.find(row.first) ||
          catalog.obligation_policies.find(row.first) ||
          (catalog.fiscal_need_policies &&
           catalog.fiscal_need_policies->policies.find(row.first)))
        return {
            false,
            {},
            common_failure("DuplicateCohortWageNeedPolicy", row.first).error};
  }
  AffairsState state;
  state.cohort_wage_needs = bool(catalog.cohort_wage_need_policies);
  state.obligation_needs = catalog.obligation_needs;
  state.fiscal_needs = bool(catalog.fiscal_need_policies);
  return {true, std::move(state), {}};
}
Outcome<NeedClaimQuote>
prepare_need_claim(const CommonStore &common, const AffairsState &state,
                   const TaskState &tasks, const StableId &need_id,
                   Integer amount, const SubjectRef &assignee,
                   const StableId &executor, const StableId &offer_id,
                   Integer revision, const OrganizationRead *organizations,
                   const ActingAuthority *authority,
                   const StableId &payee_wallet_id,
                   const EconomicSupplyPolicyRead *issued_supply) {
  auto *n = state.needs.find(need_id);
  if (n && n->measure == NeedMeasure::outstanding_obligation)
    return {false,
            {},
            common_failure("UnsupportedNeedClaim", need_id,
                           ErrorKind::unsupported_capability)
                .error};
  auto *offer = tasks.offer_instances.find(offer_id);
  auto *actor = common.actor(executor);
  if (!n || !offer || offer->source_need_id != need_id)
    return {false, {}, common_failure("UnknownOpportunity").error};
  if (!actor || !actor->alive || !actor->available || assignee.id.empty() ||
      (assignee.kind == SubjectKind::actor && assignee.id != executor))
    return {false, {}, common_failure("InvalidContractAssignee").error};
  if (affair_source_suspended(state, *n))
    return {false, {}, common_failure("NeedSourceSuspended", need_id).error};
  auto issued_valid =
      validate_affairs_issued_supply_claim(common, *n, *offer, issued_supply);
  if (!issued_valid.ok)
    return {false, {}, issued_valid.error};
  if (n->revision != revision)
    return {false, {}, common_failure("StaleNeedRevision").error};
  if (terminal(n->status) || offer->status != OfferStatus::published ||
      common.tick() < offer->available_from_tick ||
      (offer->expires_tick && common.tick() > *offer->expires_tick))
    return {false, {}, common_failure("OpportunityClosed").error};
  Need current = *n;
  if (current.measure == NeedMeasure::stock_gap) {
    auto *inventory = common.inventory(current.target_inventory_id);
    if (!inventory)
      return {false, {}, common_failure("InvalidContractDestination").error};
    current.physical_amount = 0;
    for (const auto &lot : inventory->lots)
      if (lot.item_id == current.item_id &&
          owner_equal(lot.owner, inventory->owner) &&
          !add_ok(current.physical_amount, lot.quantity,
                  current.physical_amount))
        return {false, {}, common_failure("SupplyQuantityOverflow").error};
  }
  auto available = need_available_amount(state, current);
  if (!available.ok)
    return {false, {}, available.error};
  if (amount <= 0 || amount > available.value ||
      amount > offer->definition.goal.quantity)
    return {false, {}, common_failure("NeedShareUnavailable").error};
  if (assignee.kind == SubjectKind::organization) {
    if (!organizations || !authority ||
        authority->organization_id != assignee.id ||
        !organizations->actor_accepts_duty(executor, assignee.id))
      return {false, {}, common_failure("ContractAuthorityRequired").error};
    auto proof = organizations->authorize(*authority, "claim_need",
                                          offer->accept_scope, amount);
    if (!proof.ok)
      return {false, {}, proof.error};
    if (proof.value.candidate_revision != common.revision() ||
        proof.value.authority_epoch != authority->authority_epoch)
      return {false, {}, common_failure("StaleContractAuthority").error};
  } else if (assignee.kind != SubjectKind::actor)
    return {false,
            {},
            common_failure("UnsupportedContractAssignee", {},
                           ErrorKind::unsupported_capability)
                .error};
  const auto &d = offer->definition;
  if (d.template_id != "procurement" || d.goal.quantity <= 0 ||
      d.budget % d.goal.quantity)
    return {false, {}, common_failure("InvalidRuntimeTemplate").error};
  FrozenTaskTerms terms;
  terms.definition = d;
  terms.definition.goal.quantity = amount;
  terms.agreed_quantity = amount;
  if (!mul_ok(amount, d.budget / d.goal.quantity, terms.purchase_budget_cap) ||
      !add_ok(common.tick(), d.accept_ticks, terms.deadline_tick) ||
      !add_ok(terms.deadline_tick, d.deadline_ticks, terms.deadline_tick))
    return {false, {}, common_failure("ContractTermsOverflow").error};
  auto reward_quote = task_prorata(d.reward, amount, d.goal.quantity);
  if (!reward_quote.ok)
    return {false, {}, reward_quote.error};
  terms.reward_total = reward_quote.value;
  terms.definition.budget = terms.purchase_budget_cap;
  terms.definition.reward = terms.reward_total;
  terms.target_inventory_id = n->target_inventory_id;
  terms.item_id = n->item_id;
  terms.quantity_unit = n->unit;
  terms.source_wallet_id = offer->funding_wallet_id;
  terms.reward_wallet_id = offer->reward_wallet_id;
  terms.payee_wallet_id = assignee.kind == SubjectKind::organization
                              ? payee_wallet_id
                              : actor->wallet_id;
  if (assignee.kind == SubjectKind::actor && !payee_wallet_id.empty() &&
      payee_wallet_id != actor->wallet_id)
    return {false, {}, common_failure("ContractPayeeMismatch").error};
  terms.beneficiary = n->beneficiary;
  terms.terms_version = offer->terms_version;
  auto *source = common.wallet(terms.source_wallet_id);
  auto *reward = common.wallet(terms.reward_wallet_id);
  auto *payee = common.wallet(terms.payee_wallet_id);
  if (!source || !reward || !payee ||
      (assignee.kind == SubjectKind::organization &&
       (payee->owner.kind != Owner::Kind::organization ||
        payee->owner.id != assignee.id)) ||
      !owner_equal(source->owner, offer->issuer) ||
      !owner_equal(reward->owner, offer->issuer))
    return {false, {}, common_failure("InvalidContractFunding").error};
  auto funds = common.spendable(source->id),
       reward_funds = common.spendable(reward->id);
  if (!funds.ok)
    return {false, {}, funds.error};
  if (!reward_funds.ok)
    return {false, {}, reward_funds.error};
  Integer total = 0;
  if (!add_ok(terms.purchase_budget_cap, terms.reward_total, total))
    return {false, {}, common_failure("ContractTermsOverflow").error};
  if ((source->id == reward->id && funds.value < total) ||
      (source->id != reward->id && (funds.value < terms.purchase_budget_cap ||
                                    reward_funds.value < terms.reward_total)))
    return {false, {}, common_failure("IssuerInsufficientFunds").error};
  NeedClaimQuote q;
  q.need_id = need_id;
  q.need_revision = revision;
  q.amount = amount;
  q.assignee = assignee;
  q.executor_actor_id = executor;
  q.offer_id = offer_id;
  q.terms = std::move(terms);
  if (authority)
    q.authority = *authority;
  q.resource_versions.push_back(
      common.read_version({ResourceKey::Kind::wallet, source->id}));
  if (source->id != reward->id)
    q.resource_versions.push_back(
        common.read_version({ResourceKey::Kind::wallet, reward->id}));
  return {true, std::move(q), {}};
}
Status release_claim(AffairsState &state, const StableId &id,
                     ClaimStatus reason) {
  auto *claim = state.claims.find(id);
  if (!claim)
    return common_failure("UnknownNeedClaim", id);
  if (reason != ClaimStatus::released && reason != ClaimStatus::expired)
    return common_failure("InvalidClaimRelease", id);
  if (claim->status == ClaimStatus::settled || claim->status == reason)
    return common_success();
  claim->status = reason;
  auto *need = state.needs.find(claim->need_id);
  if (!need || need->revision == std::numeric_limits<Integer>::max())
    return common_failure("NeedRevisionOverflow", id);
  ++need->revision;
  return common_success();
}
Status apply_contribution(AffairsState &state, const TaskState &tasks,
                          const DeliveryFact &delivery) {
  if (delivery.quantity_source_kind !=
          TaskQuantitySourceKind::legacy_procurement ||
      delivery.completed_at || delivery.original_completion_result_ref)
    return common_failure("ExactQuantityConsumerRequired", delivery.id,
                          ErrorKind::unsupported_capability);
  auto *actual = tasks.deliveries.find(delivery.id);
  auto *task = tasks.tasks.find(delivery.task_id);
  auto *contract = task && task->contract_id
                       ? tasks.contracts.find(*task->contract_id)
                       : nullptr;
  auto *transfer = tasks.transfers.find(delivery.transfer_fact_id);
  if (!actual || actual != &delivery) {
    if (!actual || actual->transfer_fact_id != delivery.transfer_fact_id ||
        actual->quantity != delivery.quantity ||
        actual->task_id != delivery.task_id)
      return common_failure("UnregisteredDeliveryFact", delivery.id);
  }
  if (!contract || !transfer ||
      task->record_kind != TaskRecordKind::legacy_quantity ||
      task->exact_quantity ||
      transfer->quantity_source_kind !=
          TaskQuantitySourceKind::legacy_procurement ||
      transfer->completed_at || transfer->original_completion_result_ref)
    return common_failure("UnboundLegacyDeliveryFact", delivery.id);
  const auto &binding = contract->source_binding;
  auto *need = state.needs.find(binding.need_id);
  auto *claim = state.claims.find(binding.claim_id);
  if (!need || !claim || claim->contract_id != contract->id ||
      claim->need_id != need->id || need->affair_id != binding.affair_id ||
      binding.contract_id != contract->id)
    return common_failure("DeliverySourceMismatch", delivery.id);
  if (state.exact_claims) {
    const auto *legacy =
        claim->accepted_source
            ? std::get_if<LegacyClaimAcceptedTick>(&*claim->accepted_source)
            : nullptr;
    if (!legacy || legacy->accepted_tick != claim->accepted_tick)
      return common_failure("LegacyDeliveryClaimBranchMismatch", delivery.id);
  } else if (claim->accepted_source)
    return common_failure("UnexpectedLegacyClaimSource", delivery.id);
  const auto &terms = contract->frozen_terms;
  if (delivery.activity_id != contract->org_activity_id.value_or(StableId{}) ||
      delivery.quantity <= 0 ||
      delivery.completed_tick < claim->accepted_tick ||
      delivery.to_inventory_id != terms.target_inventory_id ||
      delivery.item_id != terms.item_id ||
      !same_subject(delivery.beneficiary, terms.beneficiary) ||
      !same_subject(need->beneficiary, terms.beneficiary) ||
      transfer->contract_id != contract->id ||
      transfer->task_id != delivery.task_id ||
      transfer->source_action_id != delivery.source_action_id ||
      transfer->from_inventory_id != delivery.from_inventory_id ||
      transfer->to_inventory_id != delivery.to_inventory_id ||
      transfer->item_id != delivery.item_id ||
      transfer->quantity != delivery.quantity ||
      transfer->completed_tick != delivery.completed_tick ||
      transfer->purpose != "delivery" ||
      !owner_equal(transfer->to_owner, contract->issuer))
    return common_failure("DeliveryAcceptanceMismatch", delivery.id);
  if (auto *used = state.contributions.find(delivery.id)) {
    return used->transfer_fact_id == delivery.transfer_fact_id &&
                   used->contract_id == contract->id &&
                   used->claim_id == claim->id && used->need_id == need->id &&
                   used->counted_amount == delivery.quantity
               ? common_success()
               : common_failure("DeliveryAlreadyContributed", delivery.id);
  }
  for (const auto &row : state.contributions.rows())
    if (row.second.transfer_fact_id == delivery.transfer_fact_id)
      return common_failure("TransferAlreadyContributed",
                            delivery.transfer_fact_id);
  if (delivery.quantity > claim->accepted_amount - claim->fulfilled_amount)
    return common_failure("DeliveryExceedsClaim", delivery.id);
  Integer fulfilled = 0;
  if (!add_ok(need->fulfilled_amount, delivery.quantity, fulfilled) ||
      need->revision == std::numeric_limits<Integer>::max())
    return common_failure("NeedQuantityOverflow", need->id);
  claim->fulfilled_amount += delivery.quantity;
  if (claim->fulfilled_amount == claim->accepted_amount)
    claim->status = ClaimStatus::settled;
  else if (occupying(claim->status))
    claim->status = ClaimStatus::in_progress;
  need->fulfilled_amount = fulfilled;
  ++need->revision;
  need->receipt_ids.push_back(delivery.id);
  state.contributions.insert(
      delivery.id, {delivery.id, delivery.transfer_fact_id, contract->id,
                    claim->id, need->id, delivery.quantity});
  return common_success();
}
Status reconcile_affairs(
    CommonStore &common, AffairsState &state, const AffairsCatalog &catalog,
    TaskState &tasks, const std::vector<SupplyObservation> &observations,
    const DynamicResultRead &results, Integer tick,
    const TaskExecutionServices *execution,
    const std::vector<EconomicSupplySuspension> *supply_suspensions,
    const std::set<StableId> *issued_suspended_policy_ids,
    const DynamicStepInput *original_calendar_phase) {
  if (original_calendar_phase) {
    if (common.tick() == std::numeric_limits<Integer>::max() ||
        original_calendar_phase->from_tick != common.tick() ||
        original_calendar_phase->to_tick != common.tick() + 1 ||
        tick != original_calendar_phase->to_tick)
      return common_failure("AffairsCalendarPhaseMismatch");
  } else if (tick != common.tick())
    return common_failure("AffairsTickMismatch");
  auto suspended = read_affairs_suspended_policies(
      common, catalog, supply_suspensions, issued_suspended_policy_ids);
  if (!suspended.ok)
    return {false, {}, suspended.error};
  if (state.obligation_needs != catalog.obligation_needs)
    return common_failure("AffairsObligationVersionMismatch");
  auto obligation_needs =
      reconcile_obligation_needs(common, state, catalog, results, tick);
  if (!obligation_needs.ok)
    return obligation_needs;
  if (state.exact_claims) {
    auto r = reconcile_quantity_contributions_v2(common, state, tasks);
    if (!r.ok)
      return r;
  } else
    for (const auto &row : tasks.deliveries.rows()) {
      auto *delivery = results.delivery(row.first);
      if (!delivery)
        return common_failure("MissingDynamicDelivery",
                              row.first); // 后续正常阶段重新读原结果再发有限通知；旧
                                          // revision 不替换原成果或重复贡献。
      auto notice = read_task_delivery_objective_result(common, tasks,
                                                        delivery->id, tick);
      if (!notice.ok)
        return {false, {}, notice.error};
      auto r = apply_objective_result_committed_in_candidate(
          common, state, tasks, notice.value, tick);
      if (!r.ok)
        return r;
    }
  auto contracts = reconcile_task_contracts(common, tasks, state, tick,
                                            execution, original_calendar_phase);
  if (!contracts.ok)
    return contracts;
  std::set<StableId> seen;
  for (const auto &observation : observations) {
    if (!seen.insert(observation.source_key).second)
      return common_failure("DuplicateSupplyObservation",
                            observation.source_key);
    const NeedPolicy *policy = nullptr;
    for (const auto &row : catalog.policies.rows())
      if (row.second.source_key == observation.source_key) {
        policy = &row.second;
        break;
      }
    if (!policy)
      continue;
    if (suspended.value.count(policy->id))
      return common_failure("SuspendedSupplyObservation",
                            observation.source_key);
    auto valid = policy_valid(*policy);
    if (!valid.ok)
      return valid;
    auto *inventory = common.inventory(observation.inventory_id);
    if (!inventory || observation.organization_id != policy->organization_id ||
        observation.site_id != policy->site_id ||
        observation.inventory_id != policy->inventory_id ||
        observation.item_id != policy->item_id ||
        observation.target_amount < 0 || observation.physical_amount < 0 ||
        observation.source_revision < 0 || observation.source_fact_id.empty() ||
        !common.state().facts.find(observation.source_fact_id))
      return common_failure("InvalidSupplyObservation", observation.source_key);
    Integer physical = 0;
    for (const auto &lot : inventory->lots)
      if (lot.item_id == observation.item_id &&
          owner_equal(lot.owner, inventory->owner) &&
          !add_ok(physical, lot.quantity, physical))
        return common_failure("SupplyQuantityOverflow", observation.source_key);
    if (physical != observation.physical_amount)
      return common_failure("StaleSupplyObservation", observation.source_key);
    Need *need = nullptr;
    const Need *latest_closed_need = nullptr;
    Integer latest_closed_episode = 0;
    Integer episode = 0;
    Integer closed = -1;
    for (auto &row : state.affairs.rows())
      if (row.second.semantic_key == policy->semantic_key) {
        episode = std::max(episode, row.second.episode);
        if (row.second.closed_tick)
          closed = std::max(closed, *row.second.closed_tick);
        for (const auto &id : row.second.need_ids) {
          auto *candidate = state.needs.find(id);
          if (candidate && candidate->policy_id == policy->id) {
            if (!terminal(candidate->status) ||
                unsettled(state, tasks, *candidate))
              need = candidate;
            else if (row.second.closed_tick &&
                     row.second.episode > latest_closed_episode) {
              latest_closed_need = candidate;
              latest_closed_episode = row.second.episode;
            }
          }
        }
      }
    if (!need) {
      if (policy->measure == NeedMeasure::cumulative_delivery &&
          latest_closed_need &&
          (observation.source_revision <= latest_closed_need->source_revision ||
           observation.source_fact_id == latest_closed_need->source_fact_id))
        continue;
      const auto initial_gap =
          policy->measure == NeedMeasure::stock_gap
              ? std::max<Integer>(0, observation.target_amount -
                                         observation.physical_amount)
              : observation.target_amount;
      if (initial_gap == 0 || closed >= tick)
        continue;
      if (episode == std::numeric_limits<Integer>::max())
        return common_failure("AffairEpisodeOverflow");
      auto affair_id = common.next_id("affair");
      if (!affair_id.ok)
        return {false, {}, affair_id.error};
      auto need_id = common.next_id("need");
      if (!need_id.ok)
        return {false, {}, need_id.error};
      WorldAffair affair;
      affair.id = affair_id.value;
      affair.semantic_key = policy->semantic_key;
      affair.episode = episode + 1;
      affair.principal = {SubjectKind::organization, policy->organization_id};
      affair.cause_fact_ids = {observation.source_fact_id};
      affair.opened_tick = tick;
      affair.need_ids = {need_id.value};
      state.affairs.insert(affair.id, affair);
      Need created;
      created.id = need_id.value;
      created.affair_id = affair_id.value;
      created.source_domain = policy->source_domain;
      created.source_key = policy->source_key;
      created.beneficiary = {SubjectKind::organization,
                             policy->organization_id};
      created.target_inventory_id = policy->inventory_id;
      created.item_id = policy->item_id;
      created.unit = policy->unit;
      created.measure = policy->measure;
      created.created_tick = tick;
      created.policy_id = policy->id;
      created.target_amount = observation.target_amount;
      state.needs.insert(created.id, created);
      need = state.needs.find(need_id.value);
    }
    if (observation.source_revision < need->source_revision)
      return common_failure("StaleSupplyRevision", observation.source_key);
    const bool changed = need->physical_amount != observation.physical_amount ||
                         need->source_revision != observation.source_revision ||
                         (need->measure == NeedMeasure::stock_gap &&
                          need->target_amount != observation.target_amount);
    if (changed) {
      if (need->revision == std::numeric_limits<Integer>::max())
        return common_failure("NeedRevisionOverflow", need->id);
      ++need->revision;
    }
    if (need->measure == NeedMeasure::stock_gap)
      need->target_amount = observation.target_amount;
    need->physical_amount = observation.physical_amount;
    need->source_revision = observation.source_revision;
    need->source_fact_id = observation.source_fact_id;
    need->last_observed_tick = tick;
  }
  for (auto &row : state.needs.rows()) {
    if (row.second.measure == NeedMeasure::outstanding_obligation)
      continue;
    auto r = update_need(state, tasks, row.second, tick);
    if (!r.ok)
      return r;
  }
  for (auto &row : state.affairs.rows()) {
    auto &affair = row.second;
    if (affair.closed_tick)
      continue;
    bool closed = true, claimed = false, source_suspended = false;
    for (const auto &id : affair.need_ids) {
      auto *need = state.needs.find(id);
      if (!need) {
        if (state.state_needs && state.state_needs->needs.find(id)) {
          closed = false;
          claimed =
              claimed ||
              state.state_needs->needs.find(id)->active_claim_id.has_value();
          continue;
        }
        return common_failure("MissingAffairNeed", id);
      }
      if (!terminal(need->status) || unsettled(state, tasks, *need))
        closed = false;
      claimed = claimed || !need->claim_ids.empty();
      source_suspended =
          source_suspended || suspended.value.count(need->policy_id);
    }
    if (closed) {
      affair.phase = AffairPhase::resolved;
      affair.closed_tick = tick;
    } else
      affair.phase = source_suspended
                         ? AffairPhase::suspended
                         : (claimed ? AffairPhase::active : AffairPhase::open);
  }
  return common_success();
}
Status publish_need_offers(CommonStore &common, AffairsState &state,
                           const AffairsCatalog &policies, TaskState &tasks,
                           const TaskCatalog &catalog, Integer tick) {
  for (const auto &row : state.needs.rows()) {
    const auto &need = row.second;
    if (need.measure == NeedMeasure::outstanding_obligation)
      continue;
    auto *policy = policies.policies.find(need.policy_id);
    if (!policy)
      return common_failure("MissingNeedPolicy", need.id);
    OfferInstance *current = nullptr;
    for (auto &offer : tasks.offer_instances.rows())
      if (offer.second.source_need_id == need.id &&
          offer.second.template_id == policy->template_id &&
          offer.second.terms_version == policy->terms_version &&
          offer.second.issuer.kind == Owner::Kind::organization &&
          offer.second.issuer.id == policy->organization_id) {
        if (current)
          return common_failure("DuplicateRuntimeOffer", need.id);
        current = &offer.second;
      }
    if (terminal(need.status)) {
      if (current)
        current->status = OfferStatus::closed;
      continue;
    }
    if (affair_source_suspended(state, need)) {
      if (current)
        current->status = OfferStatus::withdrawn;
      continue;
    }
    if (current) {
      current->status =
          need_gap(need) == 0 ? OfferStatus::withdrawn : OfferStatus::published;
      continue;
    }
    auto *definition = catalog.templates.find(policy->template_id);
    if (!definition || definition->template_id != "procurement")
      return common_failure("UnknownDynamicTaskTemplate", policy->template_id);
    auto id = common.next_id("opportunity");
    if (!id.ok)
      return {false, {}, id.error};
    OfferInstance offer;
    offer.id = id.value;
    offer.template_id = policy->template_id;
    offer.source_need_id = need.id;
    offer.need_revision_at_creation = need.revision;
    offer.issuer = {Owner::Kind::organization, policy->organization_id, {}};
    offer.accept_scope = {ScopeRef::Kind::site, policy->site_id};
    offer.report_scope = offer.accept_scope;
    offer.funding_wallet_id = policy->funding_wallet_id;
    offer.reward_wallet_id = policy->reward_wallet_id;
    offer.terms_version = policy->terms_version;
    offer.available_from_tick = tick;
    offer.definition = *definition;
    offer.definition.id = offer.id;
    offer.definition.issuer = offer.issuer;
    offer.definition.funding_wallet = policy->funding_wallet_id;
    offer.definition.accept_role = policy->accept_role;
    offer.definition.report_role = policy->report_role;
    offer.definition.deadline_ticks = policy->deadline_ticks;
    offer.definition.goal.item_id = policy->item_id;
    offer.definition.goal.receiver_role = policy->report_role;
    offer.definition.goal.quantity = policy->max_claim_quantity;
    if (!mul_ok(policy->max_claim_quantity, policy->purchase_budget_per_unit,
                offer.definition.budget) ||
        !mul_ok(policy->max_claim_quantity, policy->reward_per_unit,
                offer.definition.reward))
      return common_failure("ContractTermsOverflow");
    if (policy->reward_total)
      offer.definition.reward = *policy->reward_total;
    tasks.offer_instances.insert(offer.id, offer);
  }
  return common_success();
}
} // namespace hanmo::resident
