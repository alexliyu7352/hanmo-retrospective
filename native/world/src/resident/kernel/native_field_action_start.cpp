#include "native_field_action_start.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/military/formation_exact.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "resident/domains/military/office_exact.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "resident/storage/resource_execution_coverage.hpp"
#include <optional>
namespace hanmo::resident {
bool native_field_elapsed_action_selected(
    const WorldBundle &w, const MilitaryCommand &command) noexcept {
  return w.catalog && w.catalog->field_battle && w.catalog->exact_time &&
         exact_complete_selected(*w.catalog->exact_time) &&
         (std::holds_alternative<OfficeAccept>(command) ||
          std::holds_alternative<ArmyForm>(command));
}
bool native_field_original_military_quote_selected(
    const WorldBundle &w, const MilitaryCommand &command) noexcept {
  return native_field_elapsed_action_selected(w, command) ||
         (w.catalog && w.catalog->field_battle && w.catalog->exact_time &&
          exact_complete_selected(*w.catalog->exact_time) &&
          std::holds_alternative<CampaignAccept>(command));
}
Outcome<MilitaryQuote>
preview_native_field_original_military_command(const WorldBundle &w,
                                               const StableId &actor,
                                               const MilitaryCommand &command) {
  if (!native_field_original_military_quote_selected(w, command))
    return {false,
            {},
            common_failure("FieldMilitaryOriginalQuoteSelectionRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto at = w.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  const auto factory = native_military_exact_world_factory();
  if (!factory.read)
    return {false,
            {},
            common_failure("FieldMilitaryOriginalReadRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto lease = factory.read(w, at.value);
  if (!lease.ok)
    return {false, {}, std::move(lease.error)};
  if (!lease.value)
    return {false,
            {},
            common_failure("FieldMilitaryOriginalReadRequired", {},
                           ErrorKind::internal)
                .error};
  const auto binding = lease.value->bindings();
  if (&binding.common != &w.common || &binding.military != &w.military ||
      &binding.catalog != w.catalog->military.get() ||
      !binding.access.interaction_access || !binding.access.authority_time)
    return {false,
            {},
            common_failure("FieldMilitaryOriginalReadMismatch", {},
                           ErrorKind::internal)
                .error};
  return preview_military(w.common, w.military, binding.catalog, binding.access,
                          actor, command);
}
Outcome<MilitaryEffect> apply_native_field_campaign_acceptance_in_candidate(
    WorldBundle &w, const StableId &actor, const CampaignAccept &command) {
  if (!native_field_original_military_quote_selected(
          w, MilitaryCommand{command}) ||
      !w.active_exact_write_context() || !w.read_stamps ||
      !w.common.actor_condition_capture_identity() ||
      !w.common.actor_location_capture_identity())
    return {false,
            {},
            common_failure("FieldOrderActualCaptureRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto at = w.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  // phase 先于 lease 声明：全部原权限/参与读一直借到真实受令头落完。
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!w.has_active_read_phase()) {
    auto entered = w.enter_phase_read_context(
        "field_campaign_accept", "military", {at.value, at.value}, at.value);
    if (!entered.ok)
      return {false, {}, std::move(entered.error)};
    phase = std::move(entered.value);
  }
  const auto factory = native_military_exact_world_factory();
  if (!factory.write)
    return {false,
            {},
            common_failure("FieldOrderOriginalWriterRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto lease = factory.write(w, at.value, *w.active_exact_write_context());
  if (!lease.ok)
    return {false, {}, std::move(lease.error)};
  if (!lease.value)
    return {false,
            {},
            common_failure("FieldOrderOriginalWriterRequired", {},
                           ErrorKind::internal)
                .error};
  const auto binding = lease.value->bindings();
  if (&binding.common != &w.common || &binding.military != &w.military ||
      &binding.read.common != &w.common ||
      &binding.read.military != &w.military ||
      &binding.read.catalog != w.catalog->military.get() ||
      binding.read.access.military_authority_time_schema != 1 ||
      !binding.read.access.authority_time ||
      !binding.read.access.interaction_access)
    return {false,
            {},
            common_failure("FieldOrderOriginalWriterMismatch", {},
                           ErrorKind::internal)
                .error};
  ReadStampWriteBoundary boundary(*w.read_stamps);
  return apply_military_instant(w.common, w.military, binding.read.catalog,
                                binding.read.access, actor,
                                MilitaryCommand{command});
}
Outcome<std::shared_ptr<MilitaryExactWriteLease>>
prepare_native_field_elapsed_action_start(WorldBundle &w,
                                          const MilitaryCommand &command) {
  if (!native_field_elapsed_action_selected(w, command) ||
      !w.active_exact_write_context() ||
      !w.common.actor_condition_capture_identity() ||
      !w.common.actor_location_capture_identity() || !w.military_action_time ||
      !w.action_elapsed)
    return {false,
            {},
            common_failure("FieldActionActualCaptureRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto selected = read_military_exact_phase_selection(*w.catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const bool office = std::holds_alternative<OfficeAccept>(command);
  if (!selected.value.field_original || selected.value.dynamic_schema ||
      (office ? !selected.value.actions.office_accept
              : !selected.value.actions.army_form))
    return {false,
            {},
            common_failure("FieldActionOriginalProfileRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto at = w.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  const auto factory = native_military_exact_world_factory();
  if (!factory.write)
    return {false,
            {},
            common_failure("FieldActionOriginalWriterRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  return factory.write(w, at.value, *w.active_exact_write_context());
}
Status start_native_field_elapsed_action_in_candidate(
    WorldBundle &w, MilitaryExactWriteLease &lease,
    const ActionCoreView &action, const MilitaryCommand &command,
    const MilitaryQuote &quote) {
  if (!native_field_elapsed_action_selected(w, command) ||
      !w.military_action_time || !w.action_elapsed)
    return common_failure("FieldActionOriginalSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  auto binding = lease.bindings();
  if (&binding.common != &w.common || &binding.military != &w.military ||
      &binding.elapsed != &*w.action_elapsed ||
      binding.action_time != &*w.military_action_time ||
      &binding.read.common != &w.common ||
      binding.read.action_time != &*w.military_action_time || binding.dynamic ||
      binding.read.dynamic)
    return common_failure("FieldActionOriginalLeaseMismatch", {},
                          ErrorKind::internal);
  // 仅原 ArmyForm 预约需要资源捕获；已有捕获只借，不能夺取外层终结权。
  std::optional<ResourceExecutionCoverageBoundary> formation_coverage;
  Outcome<StableId> result;
  if (const auto *request = std::get_if<OfficeAccept>(&command)) {
    if (action.kind != "office_accept" || !binding.office)
      return common_failure("FieldOfficeOriginalWriterRequired", {},
                            ErrorKind::unsupported_capability);
    result = start_exact_office_action_in_candidate(
        w.common, w.military, *w.military_action_time, *w.action_elapsed,
        *binding.office, action, *request, quote);
  } else if (const auto *request = std::get_if<ArmyForm>(&command)) {
    if (action.kind != "army_form" || !binding.formation)
      return common_failure("FieldFormationOriginalWriterRequired", {},
                            ErrorKind::unsupported_capability);
    if (binding.formation->read.access.military_formation_reservations_schema &&
        !w.common.resource_execution_capture_identity()) {
      auto opened = begin_resource_execution_coverage(w.common);
      if (!opened.ok)
        return {false, {}, std::move(opened.error)};
      formation_coverage.emplace(std::move(opened.value));
    }
    result = start_exact_formation_action_in_candidate(
        w.common, w.military, *w.military_action_time, *w.action_elapsed,
        *binding.formation, action, *request, quote);
  } else
    return common_failure("FieldActionOriginalKindRequired", {},
                          ErrorKind::internal);
  if (!result.ok)
    return {false, {}, std::move(result.error)};
  if (result.value != action.id)
    return common_failure("FieldActionActualSourceMismatch", action.id,
                          ErrorKind::internal);
  // 原预约正文及 typed 来源已经深拥有；覆盖原全部资源后才允许候选继续。
  // 失败撤整个候选，不补空来源、不在析构中关闭捕获。
  if (formation_coverage) {
    auto covered = finish_resource_execution_coverage(w.common,
                                                      *formation_coverage);
    if (!covered.ok)
      return {false, {}, std::move(covered.error)};
  }
  return common_success();
}
} // namespace hanmo::resident
