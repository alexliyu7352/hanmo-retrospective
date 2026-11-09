#include "political_budget_responsibility.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
PublicBudgetResponsibilityRead::PublicBudgetResponsibilityRead(
    const WorldBundle &w, const BudgetExecutionState &e,
    const MaterialAidReadContext &a, const GovernanceWorkExpenseReadContext &g)
    : world_(w), execution_(e), aid_(a), work_(g), aid_read_(a), work_read_(g) {
}
Status PublicBudgetResponsibilityRead::validate_roots() const {
  if (execution_.schema_version != 2)
    return common_failure("BudgetExecutionVersion2Required",
                          std::to_string(execution_.schema_version));
  if (!aid_.resources.bound_to(world_.common) ||
      &world_.common != &aid_.common || &world_.common != &work_.common ||
      &aid_.economy != &work_.economy ||
      &aid_.economy != &world_.local_economy ||
      !world_.governance_work_expense ||
      &work_.state != &*world_.governance_work_expense ||
      &aid_.budget != &execution_ || work_.budget_execution != &execution_)
    return common_failure("PublicBudgetResponsibilityOwnerMismatch");
  auto now = world_.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto aid_now = aid_.clock.read_current_moment();
  if (!aid_now.ok)
    return {false, {}, aid_now.error};
  auto work_now = work_.clock.read_current_moment();
  if (!work_now.ok)
    return {false, {}, work_now.error};
  auto a = compare_world_moments(now.value, aid_now.value);
  if (!a.ok)
    return {false, {}, a.error};
  auto g = compare_world_moments(now.value, work_now.value);
  if (!g.ok)
    return {false, {}, g.error};
  if (a.value || g.value)
    return common_failure("PublicBudgetResponsibilityClockMismatch");
  return common_success();
}
Outcome<ActualBudgetResponsibility>
PublicBudgetResponsibilityRead::read_budget_responsibility(
    const BudgetExecutionBinding &b) const {
  auto valid = validate_roots();
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_budget_execution_purpose(b.purpose, 2);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (b.responsibility_source_schema != 1)
    return {false,
            {},
            common_failure("UnsupportedBudgetResponsibilitySchema",
                           std::to_string(b.responsibility_source_schema))
                .error};
  if (b.purpose == BudgetExecutionPurpose::material_aid) {
    if (!same_source_ref(b.responsibility_ref, aid_schedule_commitment_source(
                                                   b.responsibility_ref.id)))
      return {false,
              {},
              common_failure("MaterialAidBudgetResponsibilityRequired",
                             b.responsibility_ref.id)
                  .error};
    return aid_read_.read_budget_responsibility(b);
  }
  return work_read_.read_budget_responsibility(b);
}
} // namespace hanmo::resident
