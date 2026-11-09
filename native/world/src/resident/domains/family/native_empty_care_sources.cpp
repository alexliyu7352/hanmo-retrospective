// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "native_empty_care_sources.hpp"
#include "care_requirements_state.hpp"
#include "child_care_relief.hpp"
#include "family_birth.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/travel/travel_dependent_state.hpp"

namespace hanmo::resident {
namespace {
// 域名与保留业务种类分别反扫，不能把孤头改到另一域后绕过真空门。
bool deferred_kind(std::string_view kind) {
  return kind.starts_with("family_birth_") ||
         kind.starts_with("family_child_care") ||
         kind.starts_with("family_care_") || kind == "family_child_matured" ||
         kind.starts_with("care_") || kind == "family_birth" ||
         kind == "birth_due" || kind == "start_care_handover" ||
         kind == "stop_care";
}
bool deferred_domain(std::string_view domain) {
  return domain == "family_birth" || domain == "care_requirements" ||
         domain == "family_care_relief";
}
bool deferred_source(const SourceRef &source) {
  return deferred_domain(source.domain) || deferred_kind(source.kind);
}
bool deferred_reservation(const ReservationRef &reservation) {
  return deferred_domain(reservation.domain_id) ||
         deferred_kind(reservation.activity_kind);
}
Status nonempty(const StableId &id = {}) {
  return common_failure("DeferredBirthCareSourceNotEmpty", id);
}
} // namespace

NativeEmptyCareSourceRead::NativeEmptyCareSourceRead(
    NativeEmptyCareSources sources) noexcept
    : sources_(sources) {}

bool NativeEmptyCareSourceRead::bound_to(
    const NativeEmptyCareSources &other) const noexcept {
  return &sources_.common == &other.common && sources_.births == other.births &&
         sources_.care == other.care && sources_.origins == other.origins &&
         sources_.elapsed == other.elapsed &&
         sources_.dependents == other.dependents &&
         sources_.child_care_relief == other.child_care_relief;
}

Status NativeEmptyCareSourceRead::validate_sources() const {
  if (const auto *b = sources_.births;
      b &&
      (!b->proposals.empty() || !b->replies.empty() || !b->closures.empty() ||
       !b->births.empty() || !b->care.empty() || !b->care_starts.empty() ||
       !b->maturations.empty() || !b->birth_due_executions.empty()))
    return nonempty();
  if (const auto *r = sources_.child_care_relief;
      r && (!r->agreements.empty() || !r->starts.empty() || !r->care.empty() ||
            !r->closures.empty()))
    return nonempty();
  // 复用原十九表未选择形状门，不造第二份表名或完整 Care 选择。
  if (sources_.care) {
    auto valid = validate_care_root_selection_shape(*sources_.care, {});
    if (!valid.ok)
      return valid;
  }
  const auto &common = sources_.common.state();
  for (const auto &row : common.facts.rows())
    if (deferred_domain(row.second.domain) || deferred_kind(row.second.kind))
      return nonempty(row.first);
  for (const auto &row : common.actions.rows())
    if (deferred_kind(row.second.kind))
      return nonempty(row.first);
  // Common 只保存成功命令；删掉业务表或事实不能留下成功缓存/历史冒真空。
  for (const auto &row : common.results.rows())
    if (row.second && deferred_kind(row.second->kind))
      return nonempty(row.first);
  for (const auto &row : common.history)
    if (deferred_kind(row.kind))
      return nonempty(row.result_id);
  for (const auto &row : common.wallets.rows())
    for (const auto &reservation : row.second.reserved)
      if (deferred_reservation(reservation.action_id))
        return nonempty(row.first);
  for (const auto &row : common.inventories.rows())
    for (const auto &lot : row.second.lots)
      if (deferred_reservation(lot.reserved_by))
        return nonempty(row.first);
  if (sources_.origins) {
    for (const auto &row :
         sources_.origins->character_origin_records.origins.rows())
      if (row.second.origin_kind == CharacterOriginRecordV2::Kind::born ||
          row.second.age_basis_ref.kind == CharacterAgeBasisRef::Kind::birth ||
          std::holds_alternative<BornCharacterSource>(row.second.source) ||
          std::holds_alternative<BirthInitialCapabilityBasis>(
              row.second.initial_capability_basis))
        return nonempty(row.first);
    for (const auto &row :
         sources_.origins->actor_start_context.initial_condition_seeds.rows())
      if (std::holds_alternative<BirthConditionSeedSource>(row.second.source))
        return nonempty(row.first);
    for (const auto &row :
         sources_.origins->character_origin_records.allocation_sources.rows())
      if (row.second.allocation_kind ==
              InitialCapabilityAllocation::Kind::birth ||
          std::holds_alternative<BirthFactRef>(row.second.original_source_ref))
        return nonempty(row.first);
    for (const auto &row :
         sources_.origins->character_origin_records.registration_sources.rows())
      if (std::holds_alternative<RegistrationExactBirth>(row.second.age_basis))
        return nonempty(row.first);
  }
  // 原依附表只含 Birth/Care 的真实关系，不把一般家庭亲属关系判成照护。
  if (sources_.dependents &&
      (!sources_.dependents->bindings.empty() ||
       !sources_.dependents->binding_change_results.empty()))
    return nonempty();
  if (sources_.elapsed) {
    for (const auto &row : sources_.elapsed->fixed_service_transitions)
      if (deferred_source(row.execution_source_ref))
        return nonempty(row.transition_id);
    for (const auto &row : sources_.elapsed->terminal_bindings)
      if (deferred_kind(row.action_kind) ||
          deferred_source(row.original_execution_source_ref))
        return nonempty(row.action_ref);
  }
  return common_success();
}

Status NativeEmptyCareSourceRead::reject_birth_or_care_request() const {
  auto valid = validate_sources();
  return valid.ok ? common_failure("NOT_IMPLEMENTED", "birth_or_care",
                                   ErrorKind::unsupported_capability)
                  : valid;
}

Outcome<StateGoalSnapshot>
NativeEmptyCareSourceRead::read_care_goal(const CareCoverageGoal &) const {
  const auto valid = reject_birth_or_care_request();
  return {false, {}, valid.error};
}

Status NativeEmptyCareSourceRead::visit_selected_care_goals(
    SelectedCareGoalVisitor &) const {
  return validate_sources();
}
} // namespace hanmo::resident
