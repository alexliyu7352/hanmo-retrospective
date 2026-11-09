#include "objective_result_committed.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <charconv>
#include <limits>

namespace hanmo::resident {
namespace {
// 沿原 Task 履约历史读取的全局分配序规则，拒绝非规范和未分配的动作引用。
std::optional<Integer> original_sequence(const StableId &id,
                                         const StableId &prefix, Integer next) {
  const auto lead = prefix + ":";
  if (id.compare(0, lead.size(), lead) != 0)
    return {};
  const auto tail = id.substr(lead.size());
  Integer value = 0;
  const auto parsed =
      std::from_chars(tail.data(), tail.data() + tail.size(), value);
  if (tail.empty() || parsed.ec != std::errc{} ||
      parsed.ptr != tail.data() + tail.size() || value <= 0 || value >= next ||
      std::to_string(value) != tail)
    return {};
  return value;
}
// 来源比较只比较原引用，不把字段相等作为新增权力或第二份成果。
bool same_subject(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
// 原 receipt 类型已有活动与贡献引用；通知不得替验工提前填写 contribution_use。
bool same_receipt(const ObjectiveReceiptRef &a, const ObjectiveReceiptRef &b) {
  return a.source_domain == b.source_domain &&
         a.receipt_kind == b.receipt_kind && a.fact_id == b.fact_id &&
         a.activity_id == b.activity_id &&
         a.contribution_use_id == b.contribution_use_id &&
         a.rule_version == b.rule_version;
}
// 通知 payload 不足以授权完成；任一源字段不符都保原候选不变。
Outcome<ObjectiveResultCommitted> fail(const char *code, const StableId &id,
                                       ErrorKind kind = ErrorKind::rule) {
  return {false, {}, common_failure(code, id, kind).error};
}
} // namespace

Outcome<ObjectiveResultCommitted>
read_task_delivery_objective_result(const CommonStore &common,
                                    const TaskState &tasks, const StableId &id,
                                    Integer through_tick) {
  const auto now = common.tick();
  if (through_tick != now &&
      (now == std::numeric_limits<Integer>::max() || through_tick != now + 1))
    return fail("ObjectiveResultBoundaryMismatch", id);
  const auto *delivery = tasks.deliveries.find(id);
  const auto *task = delivery ? tasks.tasks.find(delivery->task_id) : nullptr;
  const auto *contract = task && task->contract_id
                             ? tasks.contracts.find(*task->contract_id)
                             : nullptr;
  const auto *transfer =
      delivery ? tasks.transfers.find(delivery->transfer_fact_id) : nullptr;
  if (!delivery || !task || !contract || !transfer)
    return fail("ObjectiveResultOriginalSourceRequired", id);
  if (tasks.exact_quantities ||
      task->record_kind != TaskRecordKind::legacy_quantity ||
      task->exact_quantity ||
      delivery->quantity_source_kind !=
          TaskQuantitySourceKind::legacy_procurement ||
      transfer->quantity_source_kind !=
          TaskQuantitySourceKind::legacy_procurement ||
      delivery->completed_at || transfer->completed_at ||
      delivery->original_completion_result_ref ||
      transfer->original_completion_result_ref)
    return fail("ObjectiveResultSourceNotRegistered", id,
                ErrorKind::unsupported_capability);
  const SourceRef original{"tasks", "task_delivery", id};
  const SourceRef original_transfer{"tasks", "task_transfer", transfer->id};
  const auto position = read_actual_common_fact_position(common, original);
  if (!position.ok)
    return {false, {}, position.error};
  const auto transfer_position =
      read_actual_common_fact_position(common, original_transfer);
  if (!transfer_position.ok)
    return {false, {}, transfer_position.error};
  if (position.value >
          static_cast<std::size_t>(std::numeric_limits<Integer>::max()) ||
      transfer_position.value >= position.value)
    return fail("ObjectiveResultOriginalOrderRequired", id);
  // 与原 Task 公账历史绑定相同：续任不把过去任务改写为当前执行任务。
  if (task->id != delivery->task_id || contract->id != *task->contract_id ||
      (contract->current_task_id != task->id &&
       std::find(contract->historical_task_ids.begin(),
                 contract->historical_task_ids.end(),
                 task->id) == contract->historical_task_ids.end()))
    return fail("ObjectiveResultOriginalTaskBindingMismatch", id);
  const auto next = common.state().next_instance_id;
  const auto task_sequence = original_sequence(task->id, "task", next);
  const auto action_sequence =
      original_sequence(delivery->source_action_id, "action", next);
  const auto transfer_sequence =
      original_sequence(transfer->id, "transfer", next);
  const auto delivery_sequence = original_sequence(id, "delivery", next);
  if (!task_sequence || !action_sequence || !transfer_sequence ||
      !delivery_sequence || *task_sequence >= *action_sequence ||
      *action_sequence >= *transfer_sequence ||
      *transfer_sequence >= *delivery_sequence)
    return fail("ObjectiveResultOriginalActionOrderMismatch", id);
  // 完成写点仍有真实动作及参数，逐项绑定；旧动作由原完成尾删除，历史读不补造它。
  const auto *active = common.action(delivery->source_action_id);
  const auto *parameters = tasks.actions.find(delivery->source_action_id);
  if (active || parameters) {
    if (!active || !parameters || active->id != delivery->source_action_id ||
        active->kind != "report_offer" || active->actor_id != task->assignee ||
        parameters->task_id != task->id || parameters->restitution ||
        parameters->delivery_quantity != delivery->quantity)
      return fail("ObjectiveResultOriginalActionBindingMismatch", id);
  }
  const auto &terms = contract->frozen_terms;
  if (delivery->id != id || delivery->quantity <= 0 ||
      delivery->completed_tick < task->accepted_tick ||
      delivery->completed_tick > through_tick ||
      terms.definition.template_id != "procurement" ||
      terms.quantity_unit.empty() || !common.actor(task->assignee) ||
      delivery->source_action_id.empty() ||
      !same_subject(delivery->beneficiary, terms.beneficiary) ||
      delivery->activity_id != contract->org_activity_id.value_or(StableId{}) ||
      delivery->item_id != terms.item_id ||
      delivery->to_inventory_id != terms.target_inventory_id ||
      transfer->id != delivery->transfer_fact_id ||
      !common.inventory(transfer->from_inventory_id) ||
      !common.inventory(transfer->to_inventory_id) ||
      transfer->from_inventory_id == transfer->to_inventory_id ||
      transfer->contract_id != contract->id || transfer->task_id != task->id ||
      transfer->source_action_id != delivery->source_action_id ||
      transfer->from_inventory_id != delivery->from_inventory_id ||
      transfer->to_inventory_id != delivery->to_inventory_id ||
      transfer->item_id != delivery->item_id ||
      transfer->quantity != delivery->quantity ||
      transfer->completed_tick != delivery->completed_tick ||
      transfer->purpose != "delivery" || transfer->from_owner.extra ||
      transfer->from_owner.kind != Owner::Kind::task ||
      transfer->from_owner.id != task->id ||
      !owner_equal(transfer->to_owner, contract->issuer))
    return fail("ObjectiveResultOriginalPayloadMismatch", id);
  ObjectiveResultCommitted notice;
  notice.source_identity = common.state().identity;
  notice.source_domain = original.domain;
  notice.fact_ref = original;
  notice.actual_participant = {SubjectKind::actor, task->assignee};
  notice.beneficiary = delivery->beneficiary;
  notice.affected_refs = {notice.actual_participant, notice.beneficiary};
  notice.effective_tick = delivery->completed_tick;
  notice.sequence = static_cast<Integer>(position.value);
  notice.objective_receipt_ref.source_domain = original.domain;
  notice.objective_receipt_ref.receipt_kind = original.kind;
  notice.objective_receipt_ref.fact_id = original.id;
  if (!delivery->activity_id.empty())
    notice.objective_receipt_ref.activity_id = delivery->activity_id;
  // 旧 BundleServices::fact 对同一 task_delivery
  // 已登记此原规则版本，不借新版本掩盖旧来源。
  notice.objective_receipt_ref.rule_version = "local_society_v1";
  notice.measure = {delivery->quantity, terms.quantity_unit};
  notice.completed_tick = delivery->completed_tick;
  return {true, std::move(notice), {}};
}

Status validate_task_delivery_objective_result(
    const CommonStore &common, const TaskState &tasks,
    const ObjectiveResultCommitted &notice, Integer through_tick) {
  const auto &now = common.state().identity;
  const auto &source = notice.source_identity;
  if (source.world_id != now.world_id ||
      source.execution_epoch != now.execution_epoch || source.revision < 0 ||
      source.revision > now.revision || source.tick < 0 ||
      source.tick > now.tick)
    return common_failure("StaleObjectiveResultContext", notice.fact_ref.id);
  const auto actual = read_task_delivery_objective_result(
      common, tasks, notice.fact_ref.id, through_tick);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &value = actual.value;
  if (notice.source_domain != value.source_domain ||
      notice.event_kind != value.event_kind ||
      !same_source_ref(notice.fact_ref, value.fact_ref) ||
      notice.affected_refs.size() != value.affected_refs.size() ||
      notice.effective_tick != value.effective_tick ||
      notice.phase != value.phase || notice.sequence != value.sequence ||
      notice.source_schema != value.source_schema ||
      !same_receipt(notice.objective_receipt_ref,
                    value.objective_receipt_ref) ||
      !same_subject(notice.beneficiary, value.beneficiary) ||
      !same_subject(notice.actual_participant, value.actual_participant) ||
      notice.measure.amount != value.measure.amount ||
      notice.measure.unit_id != value.measure.unit_id ||
      notice.completed_tick != value.completed_tick)
    return common_failure("ObjectiveResultPayloadMismatch", notice.fact_ref.id);
  for (std::size_t index = 0; index < value.affected_refs.size(); ++index)
    if (!same_subject(notice.affected_refs[index], value.affected_refs[index]))
      return common_failure("ObjectiveResultPayloadMismatch",
                            notice.fact_ref.id);
  return common_success();
}
} // namespace hanmo::resident
