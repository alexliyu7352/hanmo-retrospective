// 来源局部读口直接调用原完整 Care 验证，不依当前活跃 binding 推测历史成功。
#include "care_facility_placement_source.hpp"

namespace hanmo::resident {
// 原接受/完成两类地址先严格核，再由实际 owner 验证其完整采用历史。
Outcome<CareFacilityOriginalHandoverView> read_care_facility_original_handover(
    const CareCoverageSources &sources, const StableId &accepted_arrangement_id,
    const StableId &handover_id,
    const SourceRef &actual_care_temporal_event_ref,
    const SourceRef &original_receiver_acceptance_ref) {
  const auto reject =
      [&](const char *code) -> Outcome<CareFacilityOriginalHandoverView> {
    return {false, {}, common_failure(code, handover_id).error};
  };
  auto temporal_schema =
      care_goal_transition_source_schema(actual_care_temporal_event_ref);
  if (!temporal_schema.ok)
    return {false, {}, temporal_schema.error};
  auto acceptance_schema =
      care_acceptance_source_schema(original_receiver_acceptance_ref);
  if (!acceptance_schema.ok)
    return {false, {}, acceptance_schema.error};
  if (temporal_schema.value != 3 || acceptance_schema.value != 3 ||
      accepted_arrangement_id.empty() || handover_id.empty() ||
      original_receiver_acceptance_ref.id != accepted_arrangement_id)
    return reject("CareFacilityOriginalAcceptanceMismatch");
  const auto &tables = sources.handovers.acceptance.goals.goals;
  const auto *event =
      tables.temporal_events.find(actual_care_temporal_event_ref.id);
  const auto *completed =
      event ? std::get_if<CareHandoverCompletedEvent>(&event->body) : nullptr;
  if (!event || !completed ||
      event->event_id != actual_care_temporal_event_ref.id ||
      !same_source_ref(event->original_transition_ref,
                       actual_care_temporal_event_ref) ||
      completed->handover_id != handover_id)
    return reject("CareFacilityOriginalCompletedHandoverRequired");
  // 原 reader 核完整 source closure、真实 period/费用/参与及 S1 原 placement；
  // 已结束/中断的历史 binding 仍可读，不以今日 phase 代原完成事实。
  auto original = read_care_binding_coverage_v2(sources, completed->binding_id);
  if (!original.ok)
    return {false, {}, original.error};
  const auto *execution = sources.handovers.executions.find(handover_id);
  if (!original.value.accepted || !original.value.binding ||
      original.value.handover_completed != event || !execution ||
      execution->handover_id != handover_id ||
      execution->accepted_care_terms_ref != accepted_arrangement_id ||
      original.value.accepted->accepted_id != accepted_arrangement_id ||
      original.value.binding->accepted_ref != accepted_arrangement_id ||
      !execution->completion_event_ref ||
      !same_source_ref(*execution->completion_event_ref,
                       actual_care_temporal_event_ref))
    return reject("CareFacilityOriginalHandoverChainMismatch");
  return {true,
          {original.value.accepted, execution, original.value.binding, event},
          {}};
}
} // namespace hanmo::resident
