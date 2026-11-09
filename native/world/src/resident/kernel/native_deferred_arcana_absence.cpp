#include "native_deferred_arcana_absence.hpp"
#include "world_bundle.hpp"
#include <type_traits>

namespace hanmo::resident {
namespace {
struct Failure { RuleError error; };
[[noreturn]] void unavailable(const StableId &id) {
  throw Failure{common_failure("NOT_IMPLEMENTED", id,
                               ErrorKind::unsupported_capability).error};
}
// 原 Arcana 输出种类已被 S9 预留；不能通过改写 domain 隐藏真实残留。
bool kind(std::string_view value) {
  return native_deferred_arcana_command_kind(value) ||
         value == "elixir_batch_completed" || value == "talisman_completed" ||
         value == "elixir_batch_cancelled" || value == "talisman_cancelled";
}
void source(const SourceRef &ref) {
  if (ref.domain == "arcana" || kind(ref.kind)) unavailable(ref.id);
}
void original(const LifeOriginalRef &ref) {
  std::visit([](const auto &value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, SourceRef>) source(value);
    else if constexpr (std::is_same_v<T, LifeOwnedFieldRef>) source(value.owner_ref);
  }, ref);
}
void outcome(const LifeFixedOutcomeRef &ref) {
  if (ref.kind == LifeFixedOutcomeRef::Kind::arcane_service) unavailable(ref.id);
}
void output(const ItemOutputAdoption &value) {
  if (value.source_kind == ItemOutputKind::ritual_material_output ||
      value.source_kind == ItemOutputKind::arcane_reward_output)
    unavailable(value.original_lot_id);
  source(value.key.source_ref);
}
} // namespace

bool native_deferred_arcana_command_kind(std::string_view value) noexcept {
  return value == "arcana" || value.starts_with("arcana_") ||
         value.starts_with("arcane_");
}

Status validate_native_deferred_arcana_absence(const WorldBundle &w) {
  try {
    if (!w.catalog || !w.catalog->exact_time ||
        !exact_complete_selected(*w.catalog->exact_time) || !w.action_elapsed)
      return common_failure("DeferredArcanaCompleteRootsRequired");
    // 本 World 没有 Arcana owning 槽；实际所选模块必须拒绝，不能补一张空表。
    for (const auto &module : w.catalog->identity.module_versions)
      if (native_deferred_arcana_command_kind(module.first) ||
          module.first == "infusion_jobs") unavailable(module.first);
    for (const auto &phase : w.catalog->exact_time->complete->phase_providers)
      if (phase.provider == ExactProviderId::arcana) unavailable(phase.profile_id);
    for (const auto &action : w.catalog->exact_time->complete->action_time_profiles)
      if (kind(action.action_kind) || kind(action.profile_id)) unavailable(action.action_kind);
    for (const auto &slot : w.catalog->exact_time->complete->facility_claims.selected_slots)
      if (slot.provider_domain == "arcana") unavailable(slot.facility_id);
    const auto &common = w.common.state();
    for (const auto &row : common.facts.rows())
      if (row.second.domain == "arcana" || kind(row.second.kind)) unavailable(row.first);
    for (const auto &row : common.actions.rows())
      if (kind(row.second.kind)) unavailable(row.first);
    for (const auto &row : common.history)
      if (kind(row.kind)) unavailable(row.result_id);
    for (const auto &row : common.results.rows())
      if (row.second && kind(row.second->kind)) unavailable(row.first);
    for (const auto &row : w.action_elapsed->terminal_bindings) {
      if (kind(row.action_kind)) unavailable(row.action_ref);
      source(row.original_execution_source_ref);
      std::visit([](const auto &v) { source(v.source_ref); }, row.started_source_ref);
      std::visit([](const auto &v) { source(v.source_ref); }, row.ended_source_ref);
    }
    for (const auto &row : w.action_elapsed->fixed_service_transitions) {
      source(row.execution_source_ref);
      std::visit([](const auto &cause) {
        using T = std::decay_t<decltype(cause)>;
        if constexpr (std::is_same_v<T, FixedOwnerTerminated>) source(cause.owner_result_ref);
        else if constexpr (std::is_same_v<T, FixedEligibilityChanged>) source(cause.original_cause_ref);
        else source(cause.owner_source_ref);
      }, row.cause);
      for (const auto *inputs : {&row.input_basis_refs, &row.previous_input_basis_refs})
        for (const auto &input : *inputs)
          if (const auto *applied = std::get_if<FixedAppliedInput>(&input)) source(applied->source_ref);
    }
    for (const auto &row : w.growth.fixed_service_boundaries.rows()) {
      const auto &value = row.second;
      if (kind(value.action_kind)) unavailable(row.first);
      if (value.original_outcome_ref) outcome(*value.original_outcome_ref);
      if (const auto *terminal = std::get_if<LifeOriginalTerminalApplication>(&value.cause)) {
        if (const auto *trigger = std::get_if<LifeOriginalResultTrigger>(&terminal->trigger)) outcome(trigger->outcome);
        for (const auto &ref : terminal->actual_result_refs) outcome(ref);
        for (const auto &ref : terminal->remaining_responsibility_refs) original(ref);
      }
      for (const auto &permission : value.input_snapshot.accepted_permission_refs) original(permission.original_ref);
      for (const auto &facility : value.input_snapshot.facility_use_refs) source(facility.original_facility_use_ref);
      for (const auto &resource : value.input_snapshot.resource_basis_refs) original(resource.original_source_ref);
    }
    if (w.item_instances)
      for (const auto &row : w.item_instances->output_adoptions) output(row);
    if (w.lot_properties)
      for (const auto &row : w.lot_properties->output_adoptions) output(row);
    if (w.social.dynamic && w.social.dynamic->time2)
      for (const auto &row : w.social.dynamic->time2->source_time_bindings.rows()) {
        source(row.second.actual_source_ref);
        if (const auto *actual = std::get_if<SocialExactSource>(&row.second.time_source)) source(actual->original_timed_source_ref.source_ref);
        if (row.second.learned_after_common_fact) source(*row.second.learned_after_common_fact);
      }
    return common_success();
  } catch (const Failure &failure) { return {false, {}, failure.error}; }
}
} // namespace hanmo::resident
