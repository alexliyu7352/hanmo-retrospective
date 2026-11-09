#include "shop_orders/task_shop_order_sources.hpp"
// 资格从真实履约凭据派生；不把取消退款、今天空库存或经办人更换当作新的客户成功。
#include "message_handoffs/task_message_report.hpp"
#include "personal_reports/task_personal_report_detail.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/calendar_gate.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "static_assets/task_static_asset_state.hpp"
#include "task_exact_execution.hpp"
#include "task_fulfilled_clients.hpp"
#include "task_public_account_state.hpp"
#include "task_quantity_time2.hpp"
#include <charconv>
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using Result = Outcome<TaskFulfilledClients>;
struct ProvenanceFailure {
  RuleError error;
};
template <class T>
void emit_checked(TaskFulfilledClientProvenanceSink *sink,
                  const TaskFulfilledClientContext &x, const T *row) {
  if (!sink)
    return;
  auto valid = sink->checked_source(x, TaskFulfilledClientCheckedSource{row});
  if (!valid.ok)
    throw ProvenanceFailure{std::move(valid.error)};
}
// 共用实例顺序证明履约确在开店之前，不能只把终态tick改小。
std::optional<Integer> serial(const StableId &id, const StableId &prefix,
                              Integer next) {
  const auto head = prefix + ":";
  if (id.compare(0, head.size(), head) != 0)
    return {};
  const auto tail = id.substr(head.size());
  Integer value = 0;
  auto parsed = std::from_chars(tail.data(), tail.data() + tail.size(), value);
  if (tail.empty() || parsed.ec != std::errc{} ||
      parsed.ptr != tail.data() + tail.size() || value <= 0 || value >= next ||
      std::to_string(value) != tail)
    return {};
  return value;
}
bool person(const SubjectRef &value, const StableId &actor) {
  return value.kind == SubjectKind::actor && value.id == actor;
}
bool same_subject(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
// 只检查原客户存在；后来死亡/组织失势不抹去已经发生的成功。
bool client_valid(const TaskFulfilledClientContext &x, const Owner &client,
                  TaskFulfilledClientProvenanceSink *sink) {
  if (client.extra || client.id.empty())
    return false;
  if (client.kind == Owner::Kind::actor) {
    if (!x.common.actor(client.id))
      return false;
    if (sink)
      emit_checked(sink, x, x.common.state().actors.find(client.id));
    return true;
  }
  if (client.kind == Owner::Kind::organization) {
    auto found = std::find(x.common_catalog.organization_ids.begin(),
                           x.common_catalog.organization_ids.end(), client.id);
    if (found == x.common_catalog.organization_ids.end())
      return false;
    if (sink) {
      const TaskOriginalOrganizationClientProvenance source{&x.common_catalog,
                                                            &*found};
      emit_checked(sink, x, &source);
    }
    return true;
  }
  return false;
}
SubjectRef beneficiary(const Owner &owner) {
  return {owner.kind == Owner::Kind::actor ? SubjectKind::actor
                                           : SubjectKind::organization,
          owner.id};
}
// 原Common事实必须是同模块具名头，且确实处于本人该任务与终结编号之间。
struct FactBoundary {
  const CommonStore &common;
  std::optional<std::size_t> position(const StableId &id) const {
    const auto *head = common.state().facts.find(id);
    if (!head)
      return {};
    auto actual = read_actual_common_fact_position(common, *head);
    return actual.ok ? std::optional<std::size_t>{actual.value} : std::nullopt;
  }
  bool before(const StableId &a, const StableId &b) const {
    auto first = position(a), last = position(b);
    return first && last && *first < *last;
  }
  std::optional<std::size_t> before_position;
  Integer task_serial = 0, settlement_serial = 0;
  bool fact(const StableId &id, const StableId &kind,
            const StableId &prefix) const {
    const auto *header = common.state().facts.find(id);
    const auto value = serial(id, prefix, common.state().next_instance_id);
    const auto at = position(id);
    return header && header->id == id && header->domain == "tasks" &&
           header->kind == kind && value && *value > task_serial &&
           *value < settlement_serial && at &&
           (!before_position || *at < *before_position);
  }
};
// Exact quantity follows the same original client/fully-paid procurement
// criterion. Its chronology is the real terminal plus the actual Common cut.
// No quantity payload is coerced into the old Calendar validator.
Outcome<std::optional<TaskFulfilledClientSource>>
exact_client(const TaskFulfilledClientContext &x, const TaskRecord &task,
             const StableId &actor, FactBoundary &facts, Integer before_serial,
             Integer observed, TaskFulfilledClientProvenanceSink *sink) {
  using R = Outcome<std::optional<TaskFulfilledClientSource>>;
  auto fail = [&](const char *code) -> R {
    return {
        false, {}, common_failure(code, task.id, ErrorKind::internal).error};
  };
  if (!x.actual_clock)
    return {false,
            {},
            common_failure("ExactTaskClientCurrentClockRequired", task.id,
                           ErrorKind::unsupported_capability)
                .error};
  if (!task.exact_quantity || !task.contract_id || !task.accepted_at ||
      !task.exact_quantity->objective_completed_at ||
      !task.exact_quantity->reported_at ||
      !task.exact_quantity->original_acceptance_result_ref ||
      !task.exact_quantity->original_report_result_ref)
    return fail("ExactTaskClientCompletionSourcesRequired");
  const auto &meta = *task.exact_quantity;
  const auto *contract = x.tasks.contracts.find(*task.contract_id);
  if (!contract || contract->current_task_id != task.id ||
      contract->settlement_id != task.settlement_id ||
      !person(contract->assignee, actor))
    return fail("InvalidTaskClientContract");
  emit_checked(sink, x, contract);
  emit_checked(sink, x, &contract->frozen_terms.definition);
  if (contract->status != ContractStatus::settled ||
      !contract->execution_stopped || contract->stop_reason != "fulfilled" ||
      !contract->restitution_refs.empty())
    return {true, std::nullopt, {}};
  const auto &terms = contract->frozen_terms;
  if (terms.agreed_quantity <= 0 ||
      contract->delivered_quantity != terms.agreed_quantity ||
      contract->eligible_delivered_quantity != terms.agreed_quantity ||
      contract->paid_reward != terms.reward_total ||
      contract->refunded_reward != 0)
    return {true, std::nullopt, {}};
  if (terms.definition.template_id != "procurement" ||
      !owner_equal(contract->issuer, terms.definition.issuer))
    return fail("InvalidTaskClientContract");
  auto accepted = read_exact_quantity_action_completion(
      x.common, x.tasks, meta.original_acceptance_action_ref,
      *meta.original_acceptance_result_ref);
  if (!accepted.ok)
    return {false, {}, accepted.error};
  if (accepted.value.task != &task || !accepted.value.terminal->effect.success)
    return fail("ExactTaskClientAcceptanceMismatch");
  emit_checked(sink, x, &accepted.value);
  const TaskExactTerminalSource *report = nullptr;
  for (const auto &id : x.common.state().facts.order())
    if (const auto *end = x.tasks.exact_execution->terminals.find(id)) {
      const auto *start =
          x.tasks.exact_execution->starts.find(end->start_source_id);
      if (start && start->accepted_action.task_id == task.id &&
          start->action_kind == "report_offer" && end->effect.success)
        report = end;
    }
  if (!report)
    return fail("ExactTaskClientReportMissing");
  auto complete = read_exact_quantity_action_completion(
      x.common, x.tasks, report->action_id, *meta.original_report_result_ref);
  if (!complete.ok)
    return {false, {}, complete.error};
  if (complete.value.task != &task || complete.value.terminal != report)
    return fail("ExactTaskClientReportMismatch");
  emit_checked(sink, x, &complete.value);
  auto equal = compare_world_moments(*meta.reported_at, report->finished_at);
  if (!equal.ok)
    return {false, {}, equal.error};
  if (equal.value)
    return fail("ExactTaskClientReportMomentMismatch");
  auto now = x.actual_clock->read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto current = compare_world_moments(report->finished_at, now.value);
  if (!current.ok)
    return {false, {}, current.error};
  if (current.value > 0)
    return fail("ExactTaskClientFutureCompletion");
  const auto task_sequence =
                 serial(task.id, "task", x.common.state().next_instance_id),
             settlement = serial(task.settlement_id, "settlement",
                                 x.common.state().next_instance_id);
  if (!task_sequence || !settlement || *task_sequence >= *settlement)
    return fail("InvalidTaskClientCompletion");
  const auto report_position = facts.position(report->id);
  if (!report_position)
    return fail("ExactTaskClientReportOrderMissing");
  // Settlement IDs are the original Common allocated identities already used
  // by this owner. The actual report header must also precede the shop header.
  if (*settlement >= before_serial ||
      report->finished_at.half_day_tick > observed ||
      (facts.before_position && *report_position >= *facts.before_position))
    return {true, std::nullopt, {}};
  facts.task_serial = *task_sequence;
  facts.settlement_serial = *settlement;
  TaskFulfilledClientSource proof;
  proof.original_client = contract->issuer;
  proof.delivery_beneficiary = terms.beneficiary;
  proof.task_id = task.id;
  proof.settlement_id = task.settlement_id;
  proof.template_id = terms.definition.template_id;
  proof.completed_tick = report->finished_at.half_day_tick;
  proof.completed_at = report->finished_at;
  proof.completion_ref = MomentOwnerSource{
      {"tasks", "task_action_finished", report->id},
      task_exact_terminal_source_schema(x.tasks.exact_execution->version)};
  Integer total = 0;
  const DeliveryFact *last = nullptr;
  std::set<StableId> transfers;
  for (const auto &id : x.common.state().facts.order())
    if (const auto *d = x.tasks.deliveries.find(id);
        d && d->task_id == task.id) {
      const auto *t = x.tasks.transfers.find(d->transfer_fact_id);
      if (d->id != id ||
          d->quantity_source_kind !=
              TaskQuantitySourceKind::procurement_exact ||
          !d->completed_at || !d->original_completion_result_ref ||
          !facts.fact(d->id, "task_delivery", "delivery") || !t ||
          t->quantity_source_kind !=
              TaskQuantitySourceKind::procurement_exact ||
          !t->completed_at || !t->original_completion_result_ref ||
          t->original_completion_result_ref->domain !=
              d->original_completion_result_ref->domain ||
          t->original_completion_result_ref->kind !=
              d->original_completion_result_ref->kind ||
          t->original_completion_result_ref->id !=
              d->original_completion_result_ref->id ||
          !facts.fact(t->id, "task_transfer", "transfer") ||
          !transfers.insert(t->id).second || !facts.before(t->id, d->id) ||
          t->contract_id != contract->id || t->task_id != task.id ||
          t->purpose != "delivery" || t->item_id != terms.item_id ||
          d->item_id != terms.item_id ||
          d->to_inventory_id != terms.target_inventory_id ||
          t->to_inventory_id != terms.target_inventory_id || d->quantity <= 0 ||
          d->quantity > terms.agreed_quantity - total ||
          t->quantity != d->quantity ||
          !owner_equal(t->to_owner, contract->issuer) || t->from_owner.extra ||
          t->from_owner.kind != Owner::Kind::task ||
          t->from_owner.id != task.id ||
          !same_subject(d->beneficiary, terms.beneficiary))
        return fail("InvalidExactTaskClientDelivery");
      auto actual = read_exact_quantity_action_completion(
          x.common, x.tasks, d->source_action_id,
          *d->original_completion_result_ref);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (actual.value.task != &task || !actual.value.terminal->effect.success)
        return fail("ExactTaskClientDeliveryCompletionMismatch");
      auto timely = calendar_deadline_accepts(terms.deadline_tick, true,
                                              *d->completed_at);
      if (!timely.ok)
        return {false, {}, timely.error};
      if (!timely.value)
        return fail("ExactTaskClientDeliveryNotTimely");
      auto prior = compare_world_moments(*d->completed_at, report->finished_at);
      if (!prior.ok)
        return {false, {}, prior.error};
      if (prior.value > 0 || !facts.before(d->id, report->id))
        return fail("ExactTaskClientDeliveryAfterReport");
      if (facts.before_position) {
        const auto terminal = facts.position(actual.value.terminal->id);
        if (!terminal || *terminal >= *facts.before_position)
          return fail("ExactTaskClientCompletionAfterOpening");
      }
      total += d->quantity;
      last = d;
      emit_checked(sink, x, &actual.value);
      emit_checked(sink, x, t);
      emit_checked(sink, x, d);
      proof.objective_fact_ids.push_back(t->id);
      proof.objective_fact_ids.push_back(d->id);
    }
  if (total != terms.agreed_quantity || !last)
    return fail("IncompleteExactTaskClientDelivery");
  equal =
      compare_world_moments(*meta.objective_completed_at, *last->completed_at);
  if (!equal.ok)
    return {false, {}, equal.error};
  if (equal.value)
    return fail("ExactTaskClientObjectiveMomentMismatch");
  return {true, std::move(proof), {}};
}
} // namespace
static Result
read_task_fulfilled_clients_impl(const TaskFulfilledClientContext &x,
                                 const StableId &actor,
                                 const TaskFulfilledClientBoundary *before,
                                 TaskFulfilledClientProvenanceSink *sink) {
  const auto fail = [&](const StableId &code,
                        const StableId &id = StableId{}) -> Result {
    return {false, {}, common_failure(code, id).error};
  };
  if (!x.common.actor(actor))
    return fail("UnknownTaskClientObserver", actor);
  Integer observed_through = x.common.tick();
  if (x.phase) {
    if (observed_through == std::numeric_limits<Integer>::max() ||
        x.phase->from_tick != observed_through ||
        x.phase->to_tick != observed_through + 1)
      return fail("InvalidTaskClientPhase");
    observed_through = x.phase->to_tick;
  }
  if (x.actual_clock) {
    auto actual = x.actual_clock->read_current_moment();
    if (!actual.ok)
      return {false, {}, actual.error};
    if (actual.value.half_day_tick < observed_through)
      return fail("TaskClientClockBeforePublished");
    observed_through = actual.value.half_day_tick;
  }
  TaskFulfilledClients out{x.common.state().identity, actor, {}};
  FactBoundary facts{x.common, {}, 0, 0};
  const Integer next = x.common.state().next_instance_id;
  Integer before_serial = next, observed = observed_through;
  if (before) {
    const auto *head = x.common.state().facts.find(before->opening_fact_id);
    const auto order = serial(before->opening_fact_id, "private_shop", next);
    const auto at = facts.position(before->opening_fact_id);
    if (!head || head->id != before->opening_fact_id ||
        head->kind != "private_shop_opened" ||
        head->domain != "private_shops" || !order || !at ||
        before->opening_tick < 0 || before->opening_tick > observed_through)
      return fail("TaskClientHistoryBoundaryUnavailable",
                  before->opening_fact_id);
    before_serial = *order;
    facts.before_position = *at;
    observed = before->opening_tick;
  }
  if (x.payments && x.payments->version != 1 && x.payments->version != 2)
    return fail("UnsupportedTaskPublicExecutionVersion");
  if (x.static_assets && x.static_assets->version != 1)
    return fail("UnsupportedTaskStaticAssetVersion");
  if (x.messages && x.messages->version != 1 && x.messages->version != 2 &&
      x.messages->version != 3 && x.messages->version != 4)
    return fail("UnsupportedTaskMessageHandoffVersion");
  std::map<std::pair<int, StableId>, TaskFulfilledClientSource> clients;
  if (x.tasks.shop_orders) {
    auto shop_clients = read_task_shop_order_fulfilled_clients(
        x.common, x.tasks, *x.tasks.shop_orders, actor, before, x.phase);
    if (!shop_clients.ok)
      return {false, {}, shop_clients.error};
    if (sink) {
      const auto &roots = *x.tasks.shop_orders;
      for (const auto &r : roots.bindings.rows())
        emit_checked(sink, x, &r.second);
      for (const auto &r : roots.settlements.rows())
        emit_checked(sink, x, &r.second);
      for (const auto &r : roots.actions.rows())
        emit_checked(sink, x, &r.second);
      for (const auto &r : roots.interruptions.rows())
        emit_checked(sink, x, &r.second);
      if (roots.estate_cancellations)
        for (const auto &r : roots.estate_cancellations->cancellations.rows())
          emit_checked(sink, x, &r.second);
    }
    for (auto &source : shop_clients.value.clients)
      clients.emplace(
          std::make_pair(static_cast<int>(source.original_client.kind),
                         source.original_client.id),
          std::move(source));
  }
  for (const auto &row : x.tasks.tasks.rows()) {
    const auto &task = row.second;
    if (x.tasks.shop_orders && x.tasks.shop_orders->by_task.count(task.id))
      continue;
    if (task.assignee != actor || task.status != "success")
      continue;
    emit_checked(sink, x, &task);
    if (task.record_kind == TaskRecordKind::exact_quantity) {
      auto exact =
          exact_client(x, task, actor, facts, before_serial, observed, sink);
      if (!exact.ok)
        return {false, {}, exact.error};
      if (!exact.value)
        continue;
      auto proof = std::move(*exact.value);
      if (!client_valid(x, proof.original_client, sink))
        return fail("InvalidTaskClientIdentity", task.id);
      if (proof.original_client.kind == Owner::Kind::actor &&
          proof.original_client.id == actor)
        continue;
      const auto key =
          std::make_pair(static_cast<int>(proof.original_client.kind),
                         proof.original_client.id);
      auto found = clients.find(key);
      auto settlement = serial(task.settlement_id, "settlement", next);
      if (found == clients.end() ||
          *settlement <
              *serial(found->second.settlement_id, "settlement", next))
        clients[key] = std::move(proof);
      continue;
    }
    if (task.record_kind != TaskRecordKind::legacy_quantity ||
        task.exact_quantity)
      return fail("TaskClientOriginalKindRequired", task.id);
    const auto task_sequence = serial(task.id, "task", next),
               settlement = serial(task.settlement_id, "settlement", next);
    if (row.first != task.id || !task_sequence || !settlement ||
        *task_sequence >= *settlement || !task.reported_tick ||
        *task.reported_tick < task.accepted_tick ||
        *task.reported_tick > observed_through)
      return fail("InvalidTaskClientCompletion", task.id);
    // 后来才成功的任务不能为早期商铺补资格，包含同tick但晚分配的settlement。
    if (*settlement >= before_serial || *task.reported_tick > observed)
      continue;
    facts.task_serial = *task_sequence;
    facts.settlement_serial = *settlement;
    TaskFulfilledClientSource proof;
    proof.task_id = task.id;
    proof.settlement_id = task.settlement_id;
    proof.completed_tick = *task.reported_tick;
    const TaskDefinition *definition = nullptr;
    if (task.contract_id) {
      const auto *contract = x.tasks.contracts.find(*task.contract_id);
      if (!contract || contract->id != *task.contract_id ||
          contract->current_task_id != task.id ||
          contract->settlement_id != task.settlement_id ||
          !person(contract->assignee, actor))
        return fail("InvalidTaskClientContract", task.id);
      const auto &terms = contract->frozen_terms;
      definition = &terms.definition;
      emit_checked(sink, x, contract);
      emit_checked(sink, x, definition);
      if (contract->status != ContractStatus::settled ||
          !contract->execution_stopped ||
          contract->stop_reason != "fulfilled" ||
          !contract->restitution_refs.empty())
        continue;
      if (terms.agreed_quantity <= 0 ||
          contract->delivered_quantity != terms.agreed_quantity ||
          contract->eligible_delivered_quantity != terms.agreed_quantity ||
          contract->paid_reward != terms.reward_total ||
          contract->refunded_reward != 0)
        continue;
      if (definition->template_id != "procurement" ||
          !owner_equal(contract->issuer, definition->issuer))
        return fail("InvalidTaskClientContract", task.id);
      proof.original_client = contract->issuer;
      proof.delivery_beneficiary = terms.beneficiary;
      Integer total = 0;
      std::set<StableId> transfers;
      for (const auto &delivered : x.tasks.deliveries.rows())
        if (delivered.second.task_id == task.id) {
          const auto &d = delivered.second;
          const auto *transfer = x.tasks.transfers.find(d.transfer_fact_id);
          if (d.quantity_source_kind !=
                  TaskQuantitySourceKind::legacy_procurement ||
              d.completed_at || d.original_completion_result_ref || !transfer ||
              transfer->quantity_source_kind !=
                  TaskQuantitySourceKind::legacy_procurement ||
              transfer->completed_at ||
              transfer->original_completion_result_ref ||
              delivered.first != d.id ||
              !facts.fact(d.id, "task_delivery", "delivery") || !transfer ||
              transfer->id != d.transfer_fact_id ||
              !facts.fact(transfer->id, "task_transfer", "transfer") ||
              !transfers.insert(transfer->id).second ||
              !facts.before(transfer->id, d.id) ||
              transfer->contract_id != contract->id ||
              transfer->task_id != task.id || transfer->purpose != "delivery" ||
              transfer->source_action_id != d.source_action_id ||
              transfer->from_inventory_id != d.from_inventory_id ||
              transfer->to_inventory_id != terms.target_inventory_id ||
              d.to_inventory_id != terms.target_inventory_id ||
              transfer->item_id != terms.item_id ||
              d.item_id != terms.item_id || transfer->quantity != d.quantity ||
              d.quantity <= 0 || d.quantity > terms.agreed_quantity - total ||
              d.completed_tick < task.accepted_tick ||
              d.completed_tick > terms.deadline_tick ||
              d.completed_tick > *task.reported_tick ||
              transfer->completed_tick != d.completed_tick ||
              transfer->from_owner.extra ||
              transfer->from_owner.kind != Owner::Kind::task ||
              transfer->from_owner.id != task.id ||
              !owner_equal(transfer->to_owner, contract->issuer) ||
              !same_subject(d.beneficiary, terms.beneficiary))
            return fail("InvalidTaskClientDelivery", d.id);
          const auto action = serial(d.source_action_id, "action", next),
                     transfer_sequence = serial(transfer->id, "transfer", next),
                     delivery_sequence = serial(d.id, "delivery", next);
          if (!action || !transfer_sequence || !delivery_sequence ||
              *action <= *task_sequence || *action >= *transfer_sequence ||
              *transfer_sequence >= *delivery_sequence)
            return fail("InvalidTaskClientDeliveryOrder", d.id);
          total += d.quantity;
          emit_checked(sink, x, transfer);
          emit_checked(sink, x, &d);
          proof.objective_fact_ids.push_back(transfer->id);
          proof.objective_fact_ids.push_back(d.id);
        }
      if (total != terms.agreed_quantity)
        return fail("IncompleteTaskClientDelivery", task.id);
    } else {
      definition = x.catalog.offers.find(task.offer_id);
      if (!definition)
        return fail("UnknownTaskClientOffer", task.id);
      emit_checked(sink, x, definition);
      if (definition->template_id != "procurement" &&
          definition->template_id != "message" &&
          definition->template_id != "escort")
        continue;
      // 原Task2可提供成功结酬源，但私人生活也可沿已登记实物/返报原件证明完整report。
      const TaskStaticPaymentSource *payment = nullptr;
      if (x.payments && x.payments->version == 2) {
        for (const auto &paid : x.payments->static_payments.rows())
          if (paid.second.task_id == task.id &&
              paid.second.stage != TaskStaticPaymentStage::acceptance) {
            if (payment || paid.first != paid.second.fact_id)
              return fail("InvalidTaskClientPayment", task.id);
            payment = &paid.second;
          }
        if (!payment)
          return fail("MissingTaskClientPayment", task.id);
        if (payment->stage != TaskStaticPaymentStage::report_success ||
            payment->tick != *task.reported_tick ||
            !facts.fact(payment->fact_id, task_static_payment_fact_kind,
                        "task_static_payment"))
          return fail("InvalidTaskClientPayment", task.id);
        const auto pay_action =
                       serial(payment->source_action_id, "action", next),
                   pay_fact =
                       serial(payment->fact_id, "task_static_payment", next);
        if (!pay_action || !pay_fact || *pay_action <= *task_sequence ||
            *pay_action >= *pay_fact)
          return fail("InvalidTaskClientPaymentAction", task.id);
        emit_checked(sink, x, payment);
        proof.payment_fact_id = payment->fact_id;
      }
      proof.original_client = definition->issuer;
      proof.delivery_beneficiary = beneficiary(definition->issuer);
      if (definition->template_id == "procurement") {
        if (!x.static_assets)
          continue;
        const TaskStaticAssetReceipt *asset = nullptr;
        for (const auto &value : x.static_assets->receipts.rows())
          if (value.second.task_id == task.id) {
            if (asset || value.first != value.second.fact_id)
              return fail("InvalidTaskClientAssets", task.id);
            asset = &value.second;
          }
        if (!asset)
          return fail("MissingTaskClientAssets", task.id);
        if (asset->stage != "procurement_delivery" ||
            asset->offer_id != task.offer_id ||
            asset->assignee_actor_id != actor ||
            !owner_equal(asset->original_issuer, definition->issuer) ||
            !owner_equal(asset->to_container_owner, definition->issuer) ||
            asset->tick != *task.reported_tick ||
            (payment && asset->source_action_id != payment->source_action_id) ||
            !facts.fact(asset->fact_id, task_static_asset_fact_kind,
                        task_static_asset_fact_kind) ||
            (payment && !facts.before(payment->fact_id, asset->fact_id)))
          return fail("InvalidTaskClientAssets", task.id);
        const auto asset_action =
                       serial(asset->source_action_id, "action", next),
                   asset_fact = serial(asset->fact_id,
                                       task_static_asset_fact_kind, next);
        if (!asset_action || !asset_fact || *asset_action <= *task_sequence ||
            *asset_action >= *asset_fact)
          return fail("InvalidTaskClientAssetsAction", task.id);
        emit_checked(sink, x, asset);
        if (!asset->retained_task_goods.empty())
          continue;
        Integer total = 0;
        for (const auto &leg : asset->legs) {
          if (leg.item_id != definition->goal.item_id || leg.quantity <= 0 ||
              leg.quantity > definition->goal.quantity - total ||
              !owner_equal(leg.destination_owner, definition->issuer))
            return fail("InvalidTaskClientAssets", task.id);
          total += leg.quantity;
        }
        if (definition->goal.quantity <= 0 ||
            total != definition->goal.quantity)
          return fail("IncompleteTaskClientAssets", task.id);
        proof.objective_fact_ids.push_back(asset->fact_id);
      } else if (definition->template_id == "message") {
        if (!x.messages)
          continue;
        const TaskMessageHandoff *handoff = nullptr;
        for (const auto &value : x.messages->receipts.rows())
          if (value.second.task_id == task.id) {
            if (handoff || value.first != value.second.fact_id)
              return fail("InvalidTaskClientMessage", task.id);
            handoff = &value.second;
          }
        if (!handoff)
          return fail("MissingTaskClientMessage", task.id);
        if (handoff->offer_id != task.offer_id ||
            handoff->carrier_actor_id != actor ||
            handoff->recipient_actor_id != definition->goal.receiver_actor ||
            !owner_equal(handoff->original_issuer, definition->issuer) ||
            !task.objective_completed_tick ||
            *task.objective_completed_tick != handoff->tick ||
            handoff->tick > *task.reported_tick ||
            !facts.fact(handoff->fact_id, task_message_handoff_kind,
                        "task_message_handoff") ||
            (payment && !facts.before(handoff->fact_id, payment->fact_id)))
          return fail("InvalidTaskClientMessage", task.id);
        proof.delivery_beneficiary = {SubjectKind::actor,
                                      handoff->recipient_actor_id};
        emit_checked(sink, x, handoff);
        proof.objective_fact_ids.push_back(handoff->fact_id);
        if (!payment) {
          if (x.messages->version != 2 && x.messages->version != 3 &&
              x.messages->version != 4)
            continue;
          const TaskMessageReport *report = nullptr;
          for (const auto &value : x.messages->reports.rows())
            if (value.second.task_id == task.id) {
              if (report || value.first != value.second.fact_id)
                return fail("InvalidTaskClientReport", task.id);
              report = &value.second;
            }
          // 旧公约由非初始岗位人结酬时可无可靠历史听者；没有收据便不计资格。
          if (!report) {
            if (definition->issuer.kind == Owner::Kind::actor)
              return fail("MissingTaskClientReport", task.id);
            continue;
          }
          auto known = read_known_task_message_report(
              x.common, x.tasks, x.catalog, x.common_catalog, *x.messages,
              actor, report->fact_id, x.phase);
          if (!known.ok)
            return {false, {}, known.error};
          if (!known.value)
            return fail("MissingTaskClientReport", task.id);
          const auto report_serial =
              serial(report->fact_id, "task_message_report", next);
          const auto at = facts.position(report->fact_id);
          if (!report_serial || *report_serial <= *settlement || !at)
            return fail("InvalidTaskClientReport", task.id);
          if (sink) {
            // 原 known report 已完整核这两个观察者索引及个人 link；
            // 同步借原lookup，不重跑一份资格/同意规则，也不进入Social。
            const auto report_index =
                x.messages->reports_observed_by.find(actor);
            const auto handoff_index = x.messages->observed_by.find(actor);
            if (report_index == x.messages->reports_observed_by.end() ||
                handoff_index == x.messages->observed_by.end())
              return fail("TaskClientMessageProvenanceIndexMissing", task.id);
            const TaskMessageObserverProvenance ri{
                x.messages, &report_index->first, &report_index->second, true},
                hi{x.messages, &handoff_index->first, &handoff_index->second,
                   false};
            emit_checked(sink, x, &ri);
            emit_checked(sink, x, &hi);
            if (report->role_id.empty() && x.messages->version == 4) {
              const auto &original = *x.tasks.personal_reports;
              const auto *start = personal_report_detail::start_for_action(
                  original, report->source_action_id);
              const auto *end = start ? personal_report_detail::end_for_start(
                                            original, start->fact_id)
                                      : nullptr;
              if (!start || !end)
                return fail("TaskClientPersonalReportProvenanceMissing",
                            task.id);
              emit_checked(sink, x, start);
              emit_checked(sink, x, end);
            }
            if (handoff->role_id.empty() &&
                (x.messages->version == 3 || x.messages->version == 4)) {
              const auto &original = *x.tasks.personal_messages;
              const auto *start = personal_message_detail::start_for_action(
                  original, handoff->source_action_id);
              const auto *end = start ? personal_message_detail::end_for_start(
                                            original, start->fact_id)
                                      : nullptr;
              if (!start || !end)
                return fail("TaskClientPersonalMessageProvenanceMissing",
                            task.id);
              emit_checked(sink, x, start);
              emit_checked(sink, x, end);
            }
          }
          emit_checked(sink, x, report);
          if (*report_serial >= before_serial ||
              (facts.before_position && *at >= *facts.before_position))
            continue;
          proof.objective_fact_ids.push_back(report->fact_id);
        }
      } else {
        if (!payment)
          continue;
        const auto id = "escort:" + task.id;
        const auto *arrival = x.tasks.facts.find(id);
        const auto *header = x.common.state().facts.find(id);
        const auto at = facts.position(id);
        if (!arrival || !header || header->id != id ||
            header->domain != "tasks" || header->kind != "escort_arrival" ||
            !at || !facts.before(id, payment->fact_id) ||
            arrival->task_id != task.id ||
            arrival->actor_id != definition->goal.actor_id ||
            arrival->escort_actor_id != actor ||
            arrival->place_id != definition->goal.destination_place_id ||
            !task.escort || !task.escort->failure.empty() ||
            !task.objective_completed_tick ||
            *task.objective_completed_tick != arrival->tick ||
            arrival->tick != task.escort->arrival_tick ||
            arrival->tick > *task.reported_tick)
          return fail("InvalidTaskClientEscort", task.id);
        proof.delivery_beneficiary = {SubjectKind::actor,
                                      definition->goal.actor_id};
        emit_checked(sink, x, arrival);
        proof.objective_fact_ids.push_back(id);
      }
    }
    if (!client_valid(x, proof.original_client, sink))
      return fail("InvalidTaskClientIdentity", task.id);
    if (proof.original_client.kind == Owner::Kind::actor &&
        proof.original_client.id == actor)
      continue;
    proof.template_id = definition->template_id;
    const auto key = std::make_pair(
        static_cast<int>(proof.original_client.kind), proof.original_client.id);
    auto found = clients.find(key);
    if (found == clients.end() ||
        *settlement < *serial(found->second.settlement_id, "settlement", next))
      clients[key] = std::move(proof);
  }
  for (auto &row : clients)
    out.clients.push_back(std::move(row.second));
  return {true, std::move(out), {}};
}
Result read_task_fulfilled_clients(const TaskFulfilledClientContext &x,
                                   const StableId &actor,
                                   const TaskFulfilledClientBoundary *before) {
  return read_task_fulfilled_clients_impl(x, actor, before, nullptr);
}
Outcome<TaskFulfilledClients>
TaskFulfilledClientAccess::task_fulfilled_clients(const StableId &actor) const {
  return read_task_fulfilled_clients(context_, actor);
}
Outcome<TaskFulfilledClients>
TaskFulfilledClientAccess::task_fulfilled_clients_before_shop(
    const StableId &actor, const StableId &fact, Integer tick) const {
  TaskFulfilledClientBoundary before{fact, tick};
  return read_task_fulfilled_clients(context_, actor, &before);
}
Status visit_task_fulfilled_clients_before_shop_provenance(
    const TaskFulfilledClientContext &x, const StableId &actor,
    const TaskFulfilledClientBoundary &boundary,
    TaskFulfilledClientProvenanceSink &sink) {
  // 先复用唯一原算法；不把 source DTO 中成功选中的引用冒作全检查集。
  try {
    auto actual = read_task_fulfilled_clients_impl(x, actor, &boundary, &sink);
    if (!actual.ok)
      return {false, {}, actual.error};
    return sink.actual_roots(x, actor, boundary);
  } catch (const ProvenanceFailure &failure) {
    return {false, {}, failure.error};
  }
}
Status TaskFulfilledClientAccess::visit_before_shop_provenance(
    const StableId &actor, const StableId &fact, Integer tick,
    TaskFulfilledClientProvenanceSink &sink) const {


  auto memory = CheckpointMemoryCharge(0);
  TaskFulfilledClientBoundary boundary{fact, tick};
  return visit_task_fulfilled_clients_before_shop_provenance(context_, actor,
                                                             boundary, sink);
}

} // namespace hanmo::resident
