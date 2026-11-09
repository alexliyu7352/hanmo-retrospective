// 动态采购只扩展冻结合同；预算、货物、旅行和交付仍使用共同资源事务与真实个人行动。
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/affairs/objective_result_consumer.hpp"
#include "resident/domains/affairs/quantity_contribution_time2.hpp"
#include "resident/domains/director/director_model.hpp"
#include "task_model.hpp"
#include "task_personal_advance_rules.hpp"
#include "task_public_account_context.hpp"
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 整数结算不得先溢出再裁剪；比例向下取整使用有界整数长除法。
bool add_ok(Integer a, Integer b, Integer &out) {
  if (a < 0 || b < 0 || a > std::numeric_limits<Integer>::max() - b)
    return false;
  out = a + b;
  return true;
}
Owner task_owner(const StableId &id) { return {Owner::Kind::task, id, {}}; }
ReservationRef contract_ref(const StableId &id) {
  return {ReservationRef::Kind::contract, id};
}
bool contains(const std::vector<StableId> &ids, const StableId &id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}
// 只有已知机会或已知来源可报价；合法性读取真值不会自动补齐人物知识。
bool known_need(const KnownSubjectView *known,
                const InteractionState &interaction, const StableId &actor,
                const OfferInstance &offer, const Need &need) {
  if (known && known->actor_id == actor &&
      (contains(known->known_offer_ids, offer.id) ||
       contains(known->known_need_ids, need.id) ||
       contains(known->known_source_keys, need.source_key)))
    return true;
  for (const auto &person : interaction.knowledge)
    if (person.actor_id == actor)
      for (const auto &entry : person.offers)
        if (entry.offer_id == offer.id)
          return true;
  return false;
}
// 每一处受托财产保留原库存位置；远地取消只登记待清算引用。
std::vector<StableId> cargo_locations(const CommonStore &common,
                                      const ContractRecord &contract) {
  std::vector<StableId> out;
  for (const auto &inventory : common.state().inventories.rows())
    for (const auto &lot : inventory.second.lots)
      if (lot.quantity > 0 && lot.owner.kind == Owner::Kind::task &&
          (lot.owner.id == contract.current_task_id.value_or(StableId{}) ||
           contains(contract.historical_task_ids, lot.owner.id))) {
        out.push_back(inventory.first);
        break;
      }
  return out;
}
// 同一合同关闭时才形成总报酬，部分履约不会因取消抹掉此前按期贡献。
Outcome<Integer> reward_due(const ContractRecord &contract) {
  return task_prorata(contract.frozen_terms.reward_total,
                      contract.eligible_delivered_quantity,
                      contract.frozen_terms.agreed_quantity);
}
// 原数量未全部实交，但合法货损已穷尽本批货物时允许按冻结比例清算，不能靠模拟取消。
Outcome<bool> cargo_exhausted(const CommonStore &common, const TaskState &tasks,
                              const ContractRecord &contract) {
  auto totals = task_cargo_totals(tasks, contract.id);
  if (!totals.ok)
    return {false, {}, totals.error};
  const auto disposed =
      totals.value.transferred_quantity + totals.value.destroyed_quantity;
  auto remaining = task_cargo_remaining_quantity(tasks, contract.id);
  if (!remaining.ok)
    return {false, {}, remaining.error};
  return {true,
          disposed > 0 && remaining.value == 0 &&
              cargo_locations(common, contract).empty(),
          {}};
}

} // namespace
Outcome<Integer> task_prorata(Integer total, Integer quantity, Integer agreed) {
  if (total < 0 || quantity < 0 || agreed <= 0 || quantity > agreed)
    return {false, {}, common_failure("InvalidProrataTerms").error};
  const auto divisor = static_cast<std::uint64_t>(agreed);
  const auto whole = static_cast<std::uint64_t>(total) / divisor;
  const auto fraction = static_cast<std::uint64_t>(total) % divisor;
  const auto multiplier = static_cast<std::uint64_t>(quantity);
  std::uint64_t quotient = 0, remainder = 0;
  for (int bit = 62; bit >= 0; --bit) {
    quotient *= 2;
    remainder *= 2;
    if (remainder >= divisor) {
      remainder -= divisor;
      ++quotient;
    }
    if ((multiplier >> bit) & 1u) {
      quotient += whole;
      remainder += fraction;
      if (remainder >= divisor) {
        remainder -= divisor;
        ++quotient;
      }
    }
  }
  if (quotient >
      static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))
    return {false, {}, common_failure("ContractRewardOverflow").error};
  return {true, static_cast<Integer>(quotient), {}};
}
OfferRef task_command_offer(const TaskCommand &command) {
  return command.offer_ref.value_or(
      OfferRef{OfferRef::Kind::authored, command.offer_id});
}
const TaskDefinition *resolve_offer_definition(const TaskState &state,
                                               const TaskCatalog &catalog,
                                               const OfferRef &ref) {
  if (ref.kind == OfferRef::Kind::authored)
    return catalog.offers.find(ref.id);
  if (ref.kind != OfferRef::Kind::runtime)
    return nullptr;
  auto *offer = state.offer_instances.find(ref.id);
  return offer ? &offer->definition : nullptr;
}
const TaskDefinition *resolve_task_definition(const TaskState &state,
                                              const TaskCatalog &catalog,
                                              const TaskRecord &task) {
  if (task.contract_id) {
    auto *contract = state.contracts.find(*task.contract_id);
    return contract ? &contract->frozen_terms.definition : nullptr;
  }
  return resolve_offer_definition(
      state, catalog,
      task.offer_ref.value_or(
          OfferRef{OfferRef::Kind::authored, task.offer_id}));
}
const TaskDefinition *resolve_known_offer_definition(const TaskState &state,
                                                     const TaskCatalog &catalog,
                                                     const StableId &id) {
  auto *authored =
      resolve_offer_definition(state, catalog, {OfferRef::Kind::authored, id});
  auto *runtime =
      resolve_offer_definition(state, catalog, {OfferRef::Kind::runtime, id});
  return authored && runtime ? nullptr : (authored ? authored : runtime);
}
bool task_contract_reservation_exists(const TaskState &state,
                                      const StableId &id) {
  if (state_need_task_reservation_exists(state, id))
    return true;
  auto *contract = state.contracts.find(id);
  if (!contract || (contract->status != ContractStatus::accepted &&
                    contract->status != ContractStatus::active &&
                    contract->status != ContractStatus::settling))
    return false;
  const auto total = contract->frozen_terms.reward_total;
  if (total < 0 || contract->paid_reward < 0 || contract->refunded_reward < 0 ||
      contract->paid_reward > total ||
      contract->refunded_reward > total - contract->paid_reward ||
      contract->delivered_quantity < 0 ||
      contract->frozen_terms.agreed_quantity <= 0 ||
      contract->delivered_quantity > contract->frozen_terms.agreed_quantity)
    return false;
  return contract->paid_reward < total - contract->refunded_reward ||
         contract->delivered_quantity <
             contract->frozen_terms.agreed_quantity ||
         !contract->restitution_refs.empty();
}
Outcome<NeedClaimReceipt>
claim_need(CommonStore &common, AffairsState &affairs, TaskState &state,
           const NeedClaimQuote &quote, const OrganizationRead *organizations,
           const EconomicSupplyPolicyRead *issued_supply) {
  auto fresh = prepare_need_claim(
      common, affairs, state, quote.need_id, quote.amount, quote.assignee,
      quote.executor_actor_id, quote.offer_id, quote.need_revision,
      organizations, quote.authority ? &*quote.authority : nullptr,
      quote.terms.payee_wallet_id, issued_supply);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  const auto &q = fresh.value;
  auto *need = affairs.needs.find(q.need_id);
  if (need->revision == std::numeric_limits<Integer>::max())
    return {false, {}, common_failure("NeedRevisionOverflow").error};
  auto claim_id = common.next_id("claim");
  if (!claim_id.ok)
    return {false, {}, claim_id.error};
  auto contract_id = common.next_id("contract");
  if (!contract_id.ok)
    return {false, {}, contract_id.error};
  const auto serial = common.state().next_instance_id;
  auto task_id = common.next_id("task");
  if (!task_id.ok)
    return {false, {}, task_id.error};
  TaskRecord task;
  task.id = task_id.value;
  task.offer_id = q.offer_id;
  task.assignee = q.executor_actor_id;
  task.serial = serial;
  task.status = "accepting";
  task.budget_wallet_id = "budget:" + task.id;
  task.deadline_tick = q.terms.deadline_tick;
  task.offer_ref = OfferRef{OfferRef::Kind::runtime, q.offer_id};
  task.contract_id = contract_id.value;
  ContractRecord contract;
  contract.id = contract_id.value;
  contract.source_binding = {need->affair_id, need->id, claim_id.value,
                             contract.id, need->revision};
  contract.issuer = q.terms.definition.issuer;
  contract.assignee = q.assignee;
  contract.executor_actor_id = q.executor_actor_id;
  contract.current_task_id = task.id;
  contract.frozen_terms = q.terms;
  contract.accepted_authority = q.authority;
  NeedClaim claim;
  claim.id = claim_id.value;
  claim.need_id = need->id;
  claim.assignee = q.assignee;
  claim.accepted_amount = q.amount;
  claim.accepted_tick = common.tick();
  claim.expires_tick = q.terms.deadline_tick;
  claim.contract_id = contract.id;
  state.tasks.insert(task.id, task);
  state.contracts.insert(contract.id, contract);
  ResourceRecipe recipe;
  recipe.expected_versions = q.resource_versions;
  recipe.operations.push_back(
      CreateWallet{{task.budget_wallet_id, task_owner(task.id), 0, {}, {}}});
  recipe.operations.push_back(WalletDelta{q.terms.source_wallet_id,
                                          -q.terms.purchase_budget_cap, true});
  recipe.operations.push_back(
      WalletDelta{q.terms.reward_wallet_id, -q.terms.reward_total, true});
  recipe.operations.push_back(
      WalletDelta{task.budget_wallet_id,
                  q.terms.purchase_budget_cap + q.terms.reward_total, false});
  if (q.terms.reward_total > 0)
    recipe.operations.push_back(SetReservation{task.budget_wallet_id,
                                               contract_ref(contract.id),
                                               q.terms.reward_total, false});
  auto funded = common.apply_resources(recipe);
  if (!funded.ok) {
    state.tasks.erase(task.id);
    state.contracts.erase(contract.id);
    return {false, {}, funded.error};
  }
  affairs.claims.insert(claim.id, claim);
  need->claim_ids.push_back(claim.id);
  ++need->revision;
  return {true, {need->id, claim.id, contract.id, task.id, q.amount}, {}};
}
Outcome<TaskQuote> preview_dynamic_task(
    const CommonStore &common, const TaskState &state,
    const TaskCatalog &catalog, const CommonCatalog &shared,
    const InteractionState &interaction, const InteractionCatalog &interactions,
    const InteractionAccess &access, const StableId &actor,
    const TaskCommand &command, const AffairsState &affairs,
    const KnownSubjectView *known, const TaskExecutionServices *execution) {
  const auto ref = task_command_offer(command);
  const auto *definition = resolve_offer_definition(state, catalog, ref);
  auto *offer = state.offer_instances.find(ref.id);
  if (!definition || !offer || ref.kind != OfferRef::Kind::runtime ||
      (!command.offer_id.empty() && command.offer_id != ref.id))
    return {false, {}, common_failure("UnknownOffer").error};
  auto *task = task_for(state, actor, ref.id);
  if (task && task->contract_id)
    definition = resolve_task_definition(state, catalog, *task);
  if (!definition)
    return {false, {}, common_failure("InvalidTaskContract").error};
  auto contact = validate_interaction(
      common, interaction, interactions, access, actor, command.context,
      command.kind == "cancel_task" ? "report_offer" : command.kind);
  if (!contact.ok)
    return {false, {}, contact.error};
  if (contact.value.role_id != (command.kind == "accept_offer"
                                    ? definition->accept_role
                                    : definition->report_role))
    return {false, {}, common_failure("RoleNotAuthorized").error};
  if (contact.value.place_id != (command.kind == "accept_offer"
                                     ? offer->accept_scope.id
                                     : offer->report_scope.id))
    return {false, {}, common_failure("WrongContractSite").error};
  auto *person = common.actor(actor);
  if (!person)
    return {false, {}, common_failure("UnknownActor").error};
  if (command.kind == "accept_offer") {
    auto *need = affairs.needs.find(offer->source_need_id);
    if (!need || !known_need(known, interaction, actor, *offer, *need))
      return {false, {}, common_failure("SourceUnknown").error};
    if (task && task->contract_id) {
      auto *old = state.contracts.find(*task->contract_id);
      if (old && old->status != ContractStatus::settled)
        return {false, {}, common_failure("OfferAlreadyAccepted").error};
    }
    if (!command.need_revision || !command.quantity)
      return {false, {}, common_failure("MissingNeedTerms").error};
    auto claim = prepare_need_claim(
        common, affairs, state, need->id, *command.quantity,
        {SubjectKind::actor, actor}, actor, offer->id, *command.need_revision,
        nullptr, nullptr, {}, execution ? execution->issued_supply : nullptr);
    if (!claim.ok)
      return {false, {}, claim.error};
    TaskQuote quote;
    quote.duration = definition->accept_ticks;
    quote.context = contact.value;
    quote.runtime = RuntimeTaskQuote{need->id, claim.value.need_revision,
                                     claim.value.amount, claim.value.terms,
                                     claim.value.resource_versions};
    return {true, std::move(quote), {}};
  }
  if (!task || !task->contract_id)
    return {false, {}, common_failure("TaskNotActive").error};
  auto *contract = state.contracts.find(*task->contract_id);
  if (!contract || contract->status == ContractStatus::settled)
    return {false, {}, common_failure("TaskNotActive").error};
  if (command.kind == "cancel_task")
    return {true, {0, contact.value, task->id, {}, {}, {}}, {}};
  auto permission =
      read_task_execution_context(common, state, catalog, execution, task->id);
  if (!permission.ok)
    return {false, {}, permission.error};
  if (!permission.value.may_progress)
    return {false,
            {},
            common_failure(permission.value.code.empty()
                               ? "TaskPoliticalExecutionSuspended"
                               : permission.value.code)
                .error};
  if (command.kind != "report_offer" || task->status == "accepting")
    return {false, {}, common_failure("UnsupportedAction").error};
  auto remaining_quote = task_cargo_remaining_quantity(state, contract->id);
  if (!remaining_quote.ok)
    return {false, {}, remaining_quote.error};
  const auto remaining = remaining_quote.value;
  auto count = common.count(person->inventory_id, definition->goal.item_id,
                            {task_owner(task->id)});
  if (!count.ok)
    return {false, {}, count.error};
  const auto allowed = contract->execution_stopped ? count.value : remaining;
  const auto amount = command.quantity.value_or(std::min(allowed, count.value));
  if (amount <= 0 || amount > allowed || amount > count.value)
    return {false, {}, common_failure("InsufficientContractGoods").error};
  auto *item = shared.items.find(definition->goal.item_id);
  auto *destination =
      common.inventory(contract->frozen_terms.target_inventory_id);
  if (!item || !destination)
    return {false, {}, common_failure("InvalidContractDestination").error};
  auto weight = common.inventory_weight(destination->id);
  if (!weight.ok)
    return {false, {}, weight.error};
  if (item->weight < 0 || weight.value > destination->capacity ||
      (item->weight > 0 &&
       amount > (destination->capacity - weight.value) / item->weight))
    return {false, {}, common_failure("RecipientCapacity").error};
  TaskQuote quote;
  quote.duration = 1;
  quote.context = contact.value;
  quote.task_id = task->id;
  quote.reward = 0;
  quote.delivery_quantity = amount;
  quote.restitution = contract->execution_stopped;
  return {true, std::move(quote), {}};
}
Status start_dynamic_task_action(CommonStore &common, TaskState &state,
                                 const ActionCoreView &action,
                                 const TaskCommand &command,
                                 const TaskQuote &quote, AffairsState &affairs,
                                 const TaskExecutionServices *execution,
                                 const WorldMoment *exact_at) {
  if (state.exact_quantities && (!exact_at || !state.exact_execution))
    return common_failure("TaskExactQuantityExecutionRequired");
  if (affairs.exact_claims && (!exact_at || !state.claim_establishment))
    return common_failure("TaskExactAcceptanceSourceRequired");
  auto context_valid = validate_task_execution_services(state, execution);
  if (!context_valid.ok)
    return context_valid;
  if (command.kind != "accept_offer" && quote.task_id && execution) {
    auto permission = read_task_execution_context(
        common, state, *execution->catalog, execution, *quote.task_id);
    if (!permission.ok)
      return {false, {}, permission.error};
    if (!permission.value.may_progress)
      return common_failure(permission.value.code.empty()
                                ? "TaskPoliticalExecutionSuspended"
                                : permission.value.code);
  }
  TaskAction params;
  params.offer_id = task_command_offer(command).id;
  params.context = quote.context;
  params.source_kind = command.source_kind;
  params.reward = quote.reward;
  params.delivery_quantity = quote.delivery_quantity;
  params.restitution = quote.restitution;
  if (command.kind == "accept_offer") {
    if (!quote.runtime)
      return common_failure("MissingNeedPreparation");
    NeedClaimQuote q;
    q.need_id = quote.runtime->need_id;
    q.need_revision = quote.runtime->need_revision;
    q.amount = quote.runtime->quantity;
    q.assignee = {SubjectKind::actor, action.actor_id};
    q.executor_actor_id = action.actor_id;
    q.offer_id = params.offer_id;
    auto accepted = claim_need(common, affairs, state, q, nullptr,
                               execution ? execution->issued_supply : nullptr);
    if (!accepted.ok)
      return {false, {}, accepted.error};
    params.task_id = accepted.value.task_id;
    if (state.exact_quantities) {
      auto *task = state.tasks.find(params.task_id);
      auto *contract = task && task->contract_id
                           ? state.contracts.find(*task->contract_id)
                           : nullptr;
      if (!task || !contract)
        return common_failure("TaskExactQuantityAcceptedScopeRequired",
                              params.task_id, ErrorKind::internal);
      task->record_kind = TaskRecordKind::exact_quantity;
      task->exact_quantity =
          TaskExactQuantityFields{contract->assignee,
                                  action.id,
                                  {contract->frozen_terms.deadline_tick, true},
                                  {},
                                  {},
                                  {},
                                  {}};
    }
  } else if (command.kind == "report_offer") {
    if (!quote.task_id || !quote.delivery_quantity)
      return common_failure("MissingDeliveryPreparation");
    auto *task = state.tasks.find(*quote.task_id);
    auto *contract = task && task->contract_id
                         ? state.contracts.find(*task->contract_id)
                         : nullptr;
    if (!contract || contract->status == ContractStatus::settled ||
        *quote.delivery_quantity <= 0 ||
        quote.restitution != contract->execution_stopped ||
        (!quote.restitution &&
         *quote.delivery_quantity > contract->frozen_terms.agreed_quantity -
                                        contract->delivered_quantity))
      return common_failure("StaleDeliveryPreparation");
    if (!quote.restitution) {
      auto remaining = task_cargo_remaining_quantity(state, contract->id);
      if (!remaining.ok)
        return {false, {}, remaining.error};
      if (*quote.delivery_quantity > remaining.value)
        return common_failure("StaleDeliveryPreparation");
    }
    params.task_id = task->id;
    auto held = common.apply_resources(
        {{ReserveLots{common.actor(action.actor_id)->inventory_id,
                      contract->frozen_terms.item_id,
                      *quote.delivery_quantity,
                      {task_owner(task->id)},
                      action.id}},
         {}});
    if (!held.ok)
      return held;
  } else
    return common_failure("UnsupportedAction");
  state.actions.insert(action.id, std::move(params));
  if (command.kind == "accept_offer" && affairs.exact_claims) {
    auto source = establish_task_claim_source(
        common, state, affairs, state.actions.find(action.id)->task_id,
        action.id, *exact_at);
    if (!source.ok)
      return {false, {}, source.error};
  }
  return common_success();
}
Status settle_task_contract(CommonStore &common, TaskState &state,
                            const StableId &id, Integer tick,
                            const TaskExecutionServices *execution,
                            const DynamicStepInput *original_calendar_phase) {
  if (original_calendar_phase &&
      (common.tick() == std::numeric_limits<Integer>::max() ||
       original_calendar_phase->from_tick != common.tick() ||
       original_calendar_phase->to_tick != common.tick() + 1 ||
       tick != original_calendar_phase->to_tick))
    return common_failure("TaskContractCalendarPhaseMismatch");
  auto *contract = state.contracts.find(id);
  auto *task = contract && contract->current_task_id
                   ? state.tasks.find(*contract->current_task_id)
                   : nullptr;
  if (!contract || !task)
    return common_failure("InvalidTaskContract", id);
  if (task->record_kind == TaskRecordKind::native_objective)
    return common_failure("NativeObjectiveQuantitySettlementForbidden", id,
                          ErrorKind::internal);
  auto context_valid = validate_task_execution_services(state, execution);
  if (!context_valid.ok)
    return context_valid;
  if (contract->status == ContractStatus::settled)
    return common_success();
  if (!contract->execution_stopped)
    return common_success();
  if (state.objective_base && contract->stop_reason == "fulfilled") {
    bool has_objective = false;
    for (const auto &row : state.objective_base->objectives.rows())
      if (row.second.task_id == task->id) {
        has_objective = true;
        break;
      }
    if (has_objective) {
      auto ready = task_objective_settlement_ready(state, task->id);
      if (!ready.ok) {
        if (ready.error.kind == ErrorKind::rule &&
            (ready.error.original.code == "TaskObjectiveIncomplete" ||
             ready.error.original.code == "TaskObjectiveReportRequired"))
          return common_success();
        return ready;
      }
    }
  }
  // 真实本金尚未归还时，保留原货物与清算责任；本口不得先发报酬或退走托管金。
  if (contract->frozen_terms.personal_advance_allowed) {
    if (!state.personal_advances)
      return common_failure("MissingTaskAdvanceDomain", id);
    auto advance = read_task_personal_advance_summary(
        common, state, *state.personal_advances, id, original_calendar_phase);
    if (!advance.ok)
      return {false, {}, advance.error};
    if (advance.value.paid != advance.value.repaid) {
      contract->status = ContractStatus::settling;
      contract->restitution_refs = cargo_locations(common, *contract);
      task->status = "settling";
      return common_success();
    }
  }
  const auto &terms = contract->frozen_terms;
  auto *wallet = common.wallet(task->budget_wallet_id);
  if (!wallet || terms.agreed_quantity <= 0 ||
      contract->eligible_delivered_quantity > terms.agreed_quantity)
    return common_failure("InvalidContractEscrow", id);
  const auto reward_remaining =
      terms.reward_total - contract->paid_reward - contract->refunded_reward;
  if (reward_remaining < 0 || wallet->balance < reward_remaining)
    return common_failure("ContractEscrowShortfall", id);
  const auto unused_budget = wallet->balance - reward_remaining;
  auto due_quote = reward_due(*contract);
  if (!due_quote.ok)
    return {false, {}, due_quote.error};
  const auto due = due_quote.value;
  if (due < contract->paid_reward)
    return common_failure("ContractRewardRegression", id);
  const auto payment = due - contract->paid_reward;
  const auto refund_reward = reward_remaining - payment;
  if (refund_reward < 0 ||
      unused_budget > terms.purchase_budget_cap - contract->refunded_budget)
    return common_failure("InvalidContractEscrow", id);
  if (execution) {
    auto paid = pay_task_contract_in_candidate(
        common, state, *execution->catalog, *state.takeover_execution,
        *execution->politics, execution->advances, id, execution->phase);
    if (!paid.ok)
      return {false, {}, paid.error};
    if (paid.value.kind == TaskContractPaymentKind::pending) {
      contract->status = ContractStatus::settling;
      contract->restitution_refs = cargo_locations(common, *contract);
      task->status = "settling";
      return common_success();
    }
    if (paid.value.amounts.budget_return != unused_budget ||
        paid.value.amounts.reward_payment != payment ||
        paid.value.amounts.reward_return != refund_reward)
      return common_failure("TaskContractPaymentQuoteMismatch", id);
  } else {
    ResourceRecipe recipe;
    if (reward_remaining > 0)
      recipe.operations.push_back(
          SetReservation{task->budget_wallet_id, contract_ref(id), 0, true});
    if (unused_budget)
      recipe.operations.push_back(
          WalletDelta{terms.source_wallet_id, unused_budget, false});
    if (payment)
      recipe.operations.push_back(
          WalletDelta{terms.payee_wallet_id, payment, false});
    if (refund_reward)
      recipe.operations.push_back(
          WalletDelta{terms.reward_wallet_id, refund_reward, false});
    if (wallet->balance)
      recipe.operations.push_back(
          WalletDelta{task->budget_wallet_id, -wallet->balance, false});
    auto paid = common.apply_resources(recipe);
    if (!paid.ok)
      return paid;
  }
  contract->paid_reward += payment;
  contract->refunded_budget += unused_budget;
  contract->refunded_reward += refund_reward;
  contract->status = ContractStatus::settling;
  contract->restitution_refs = cargo_locations(common, *contract);
  task->status = "settling";
  if (!contract->restitution_refs.empty())
    return common_success();
  for (const auto &row : state.actions.rows())
    if (row.second.task_id == task->id && common.action(row.first))
      return common_success();
  auto settlement = common.next_id("settlement");
  if (!settlement.ok)
    return {false, {}, settlement.error};
  contract->status = ContractStatus::settled;
  contract->settlement_id = settlement.value;
  task->settlement_id = settlement.value;
  if (task->record_kind == TaskRecordKind::exact_quantity) {
    if (!task->exact_quantity)
      return common_failure(
          "TaskExactQuantityFieldsRequired", task->id,
          ErrorKind::internal); /* Actual report M and its late gate are written
                                   only by the original report Action. Delayed
                                   financial settlement cannot replace them. */
  } else {
    task->reported_tick = tick;
    task->late = tick > terms.deadline_tick;
  }
  task->status =
      contract->delivered_quantity == terms.agreed_quantity
          ? "success"
          : (contract->stop_reason == "cancelled" ? "cancelled" : "failed");
  return common_success();
}
Status cancel_dynamic_task(CommonStore &common, TaskState &state,
                           const StableId &id, AffairsState &affairs,
                           const TaskExecutionServices *execution) {
  auto *task = state.tasks.find(id);
  auto *contract = task && task->contract_id
                       ? state.contracts.find(*task->contract_id)
                       : nullptr;
  if (!contract || contract->status == ContractStatus::settled)
    return common_failure("TaskNotActive");
  if (task->record_kind == TaskRecordKind::native_objective)
    return common_failure("NativeObjectiveQuantityCancellationForbidden", id,
                          ErrorKind::internal);
  if (task->record_kind == TaskRecordKind::exact_quantity &&
      contract->deadline_closure)
    return common_failure("QuantityDeadlineCancellationPolicyRequired", id,
                          ErrorKind::unsupported_capability);
  contract->execution_stopped = true;
  contract->stop_reason = "cancelled";
  contract->status = ContractStatus::settling;
  auto release = release_claim(affairs, contract->source_binding.claim_id,
                               ClaimStatus::released);
  if (!release.ok)
    return release;
  return settle_task_contract(common, state, contract->id, common.tick(),
                              execution);
}
Outcome<TaskEffect> finish_dynamic_task_action(
    CommonStore &common, TaskState &state, const InteractionState &interaction,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const ActionCoreView &action, Integer tick, AffairsState &affairs,
    const TaskExecutionServices *execution, const WorldMoment *exact_at,
    const TaskExactLifeSources *life) {
  if (state.exact_quantities && (!exact_at || exact_at->half_day_tick != tick))
    return {false, {}, common_failure("TaskExactDeliverySourceRequired").error};
  auto *params = state.actions.find(action.id);
  auto *task = params ? state.tasks.find(params->task_id) : nullptr;
  auto *contract = task && task->contract_id
                       ? state.contracts.find(*task->contract_id)
                       : nullptr;
  if (!contract)
    return {false, {}, common_failure("InvalidTaskContract").error};
  if (state.exact_quantities &&
      (task->record_kind != TaskRecordKind::exact_quantity ||
       !task->exact_quantity))
    return {false,
            {},
            common_failure("TaskExactQuantityFieldsRequired", task->id,
                           ErrorKind::internal)
                .error};
  if (task->record_kind == TaskRecordKind::exact_quantity &&
      contract->stop_reason == "expired" &&
      ((action.kind == "accept_offer") ||
       (action.kind == "report_offer" && !params->restitution)))
    return {false,
            {},
            common_failure("QuantityDeadlineActiveActionDispositionRequired",
                           action.id, ErrorKind::unsupported_capability)
                .error};
  auto context_valid = validate_task_execution_services(state, execution);
  if (!context_valid.ok)
    return {false, {}, context_valid.error};
  if (execution) {
    auto permission = read_task_execution_context(
        common, state, *execution->catalog, execution, task->id);
    if (!permission.ok)
      return {false, {}, permission.error};
    if (!permission.value.may_progress) {
      // 真暂停只结束本次办理，不冻结全世界时钟；原受领责任转待清算而不免债。
      auto released =
          common.apply_resources({{ReleaseActionResources{action.id}}, {}});
      if (!released.ok)
        return {false, {}, released.error};
      if (action.kind == "accept_offer") {
        auto cancelled =
            cancel_dynamic_task(common, state, task->id, affairs, execution);
        if (!cancelled.ok)
          return {false, {}, cancelled.error};
      }
      TaskEffect effect;
      effect.success = false;
      effect.task_id = task->id;
      effect.code = permission.value.code.empty()
                        ? "TaskPoliticalExecutionSuspended"
                        : permission.value.code;
      effect.on_unavailable = contract->frozen_terms.definition.on_unavailable;
      effect.retry = "fresh_authorized_context";
      return {true, std::move(effect), {}};
    }
  }
  auto checked = validate_interaction(common, interaction, catalog, access,
                                      action.actor_id, params->context,
                                      action.kind, action.id);
  if (!checked.ok) {
    auto release =
        common.apply_resources({{ReleaseActionResources{action.id}}, {}});
    if (!release.ok)
      return {false, {}, release.error};
    if (action.kind == "accept_offer") {
      auto cancel =
          cancel_dynamic_task(common, state, task->id, affairs, execution);
      if (!cancel.ok)
        return {false, {}, cancel.error};
    }
    TaskEffect effect;
    effect.success = false;
    effect.task_id = task->id;
    effect.code = checked.error.original.code;
    effect.on_unavailable = contract->frozen_terms.definition.on_unavailable;
    effect.retry = "fresh_authorized_context";
    return {true, std::move(effect), {}};
  }
  if (action.kind == "accept_offer") {
    if (state.exact_quantities) {
      task->accepted_tick = 0;
      task->accepted_at = *exact_at;
    } else
      task->accepted_tick = tick;
    task->status = "active";
    contract->status = ContractStatus::active;
    auto released =
        common.apply_resources({{ReleaseActionResources{action.id}}, {}});
    if (!released.ok)
      return {false, {}, released.error};
  } else if (action.kind == "report_offer") {
    const auto amount = params->delivery_quantity.value_or(0);
    const auto &terms = contract->frozen_terms;
    if (amount <= 0 || params->restitution != contract->execution_stopped ||
        (!params->restitution &&
         amount > terms.agreed_quantity - contract->delivered_quantity))
      return {false, {}, common_failure("DeliveryExceedsContract").error};
    if (!params->restitution) {
      auto remaining = task_cargo_remaining_quantity(state, contract->id);
      if (!remaining.ok)
        return {false, {}, remaining.error};
      if (amount > remaining.value)
        return {false, {}, common_failure("DeliveryExceedsContract").error};
    }
    const auto bag = common.actor(action.actor_id)->inventory_id;
    ResourceRecipe recipe;
    recipe.operations = {TransferLots{bag,
                                      terms.target_inventory_id,
                                      terms.item_id,
                                      amount,
                                      {task_owner(task->id)},
                                      action.id,
                                      contract->issuer},
                         ReleaseActionResources{action.id}};
    auto transferred = common.apply_resources(recipe);
    if (!transferred.ok)
      return {false, {}, transferred.error};
    auto transfer_id = common.next_id("transfer");
    if (!transfer_id.ok)
      return {false, {}, transfer_id.error};
    TaskTransferFact transfer{transfer_id.value,
                              contract->id,
                              task->id,
                              action.id,
                              bag,
                              terms.target_inventory_id,
                              terms.item_id,
                              amount,
                              tick,
                              task_owner(task->id),
                              contract->issuer,
                              params->restitution ? "restitution" : "delivery"};
    auto recorded = common.append_fact({transfer.id, "task_transfer", "tasks"});
    if (!recorded.ok)
      return {false, {}, recorded.error};
    if (state.exact_quantities) {
      transfer.completed_at = *exact_at;
      transfer.quantity_source_kind = TaskQuantitySourceKind::procurement_exact;
    }
    state.transfers.insert(transfer.id, transfer);
    if (!params->restitution) {
      auto delivery_id = common.next_id("delivery");
      if (!delivery_id.ok)
        return {false, {}, delivery_id.error};
      DeliveryFact delivery{delivery_id.value,
                            task->id,
                            contract->org_activity_id.value_or(StableId{}),
                            action.id,
                            bag,
                            terms.target_inventory_id,
                            terms.beneficiary,
                            terms.item_id,
                            amount,
                            tick,
                            transfer.id};
      recorded = common.append_fact({delivery.id, "task_delivery", "tasks"});
      if (!recorded.ok)
        return {false, {}, recorded.error};
      if (state.exact_quantities) {
        delivery.completed_at = *exact_at;
        delivery.quantity_source_kind =
            TaskQuantitySourceKind::procurement_exact;
      }
      state.deliveries.insert(delivery.id, delivery);
      contract->delivered_quantity += amount;
      if (state.exact_quantities) {
        auto timely = read_task_quantity_result_deadline_eligibility(
            common, state, task->id, {"tasks", "task_delivery", delivery.id},
            life);
        if (!timely.ok)
          return {false, {}, timely.error};
        if (timely.value)
          contract->eligible_delivered_quantity += amount;
      } else if (tick <= terms.deadline_tick)
        contract->eligible_delivered_quantity += amount;
      Status contributed;
      if (state.exact_quantities) {
        contributed =
            apply_quantity_contribution_v2(common, affairs, state, delivery.id);
      } else {
        // 原实交完成点产生具名 typed
        // 通知；消费者必须重新读取两份真实结果，不能相信通知自报完成。
        auto notice = read_task_delivery_objective_result(common, state,
                                                          delivery.id, tick);
        if (!notice.ok)
          return {false, {}, notice.error};
        contributed = apply_objective_result_committed_in_candidate(
            common, affairs, state, notice.value, tick);
      }
      if (!contributed.ok)
        return {false, {}, contributed.error};
      if (contract->delivered_quantity == terms.agreed_quantity) {
        if (state.exact_quantities)
          task->exact_quantity->objective_completed_at = *exact_at;
        else
          task->objective_completed_tick = tick;
        contract->execution_stopped = true;
        contract->stop_reason = "fulfilled";
        contract->status = ContractStatus::settling;
      }
    }
    if (!contract->execution_stopped) {
      auto exhausted = cargo_exhausted(common, state, *contract);
      if (!exhausted.ok)
        return {false, {}, exhausted.error};
      if (exhausted.value) {
        contract->execution_stopped = true;
        contract->stop_reason = "cargo_loss";
        contract->status = ContractStatus::settling;
        auto released = release_claim(
            affairs, contract->source_binding.claim_id, ClaimStatus::released);
        if (!released.ok)
          return {false, {}, released.error};
      }
    }
    if (state.exact_quantities) {
      auto &q = *task->exact_quantity;
      q.reported_at = *exact_at;
      q.original_report_result_ref.reset();
      auto timely = read_task_quantity_result_deadline_eligibility(
          common, state, task->id, {"tasks", "task_transfer", transfer.id},
          life);
      if (!timely.ok)
        return {false, {}, timely.error};
      task->late = !timely.value;
    }
    if (contract->execution_stopped) {
      auto settled =
          settle_task_contract(common, state, contract->id, tick, execution);
      if (!settled.ok)
        return {false, {}, settled.error};
    }
  } else
    return {false, {}, common_failure("UnsupportedAction").error};
  TaskEffect effect;
  effect.task_id = task->id;
  return {true, std::move(effect), {}};
}
Status
reconcile_task_contracts(CommonStore &common, TaskState &state,
                         AffairsState &affairs, Integer tick,
                         const TaskExecutionServices *execution,
                         const DynamicStepInput *original_calendar_phase,
                         const TaskQuantityDeadlineContext *quantity_deadline,
                         const ExactPeriodContext *original_period) {
  for (auto &row : state.contracts.rows()) {
    auto &contract = row.second;
    const auto *task = contract.current_task_id
                           ? state.tasks.find(*contract.current_task_id)
                           : nullptr;
    if (!task || task->record_kind == TaskRecordKind::native_objective)
      return common_failure("QuantityContractTaskKindMismatch", row.first,
                            ErrorKind::internal);
    if (contract.status == ContractStatus::settled)
      continue;
    if (!contract.execution_stopped) {
      auto exhausted = cargo_exhausted(common, state, contract);
      if (!exhausted.ok)
        return {false, {}, exhausted.error};
      if (exhausted.value) {
        contract.execution_stopped = true;
        contract.stop_reason = "cargo_loss";
        contract.status = ContractStatus::settling;
        auto released = release_claim(affairs, contract.source_binding.claim_id,
                                      ClaimStatus::released);
        if (!released.ok)
          return released;
      }
    }
    if (!contract.execution_stopped &&
        tick > contract.frozen_terms.deadline_tick) {
      if (task->record_kind == TaskRecordKind::exact_quantity) {
        if (!original_calendar_phase || !quantity_deadline || !original_period)
          return common_failure("QuantityDeadlineOriginalCalendarOwnerRequired",
                                contract.id, ErrorKind::unsupported_capability);
        auto closed = record_task_quantity_deadline_close_in_candidate(
            common, state, *quantity_deadline, contract.id, *original_period);
        if (!closed.ok)
          return {false, {}, closed.error};
      }
      contract.execution_stopped = true;
      contract.stop_reason = "expired";
      contract.status = ContractStatus::settling;
      auto released = release_claim(affairs, contract.source_binding.claim_id,
                                    ClaimStatus::expired);
      if (!released.ok)
        return released;
    }
    if (contract.execution_stopped) {
      auto settled = settle_task_contract(common, state, contract.id, tick,
                                          execution, original_calendar_phase);
      if (!settled.ok)
        return settled;
    }
  }
  return common_success();
}
} // namespace hanmo::resident
