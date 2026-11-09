#include "objective_result_consumer.hpp"
#include "affairs_model.hpp"
namespace hanmo::resident {
Status apply_objective_result_committed_in_candidate(
    const CommonStore &common, AffairsState &affairs, const TaskState &tasks,
    const ObjectiveResultCommitted &notice, Integer through_tick) {
  auto valid = validate_task_delivery_objective_result(common, tasks, notice,
                                                       through_tick);
  if (!valid.ok)
    return valid;
  const auto *original =
      tasks.deliveries.find(notice.objective_receipt_ref.fact_id);
  if (!original)
    return common_failure("ObjectiveResultOriginalSourceRequired",
                          notice.fact_ref.id);
  return apply_contribution(affairs, tasks, *original);
}
} // namespace hanmo::resident
