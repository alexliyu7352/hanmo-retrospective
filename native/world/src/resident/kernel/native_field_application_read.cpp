#include "native_field_application_read.hpp"
#include "original_world_read_boundary.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
bool NativeFieldApplicationRead::bound_to(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &field) const noexcept {
  return &common == &world_.common && &military == &world_.military &&
         world_.field_battle && &field == &*world_.field_battle &&
         boundary_.bound_to(common, clock_);
}
Outcome<WorldMoment> NativeFieldApplicationRead::read_current_moment() const {
  if (!world_.catalog || !world_.field_battle ||
      !bound_to(world_.common, world_.military, *world_.field_battle))
    return {false,
            {},
            common_failure("FieldApplicationOriginalRootsRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto layout = validate_field_battle_layout(world_.catalog->identity,
                                             world_.catalog->field_battle.get(),
                                             &*world_.field_battle);
  if (!layout.ok)
    return {false, {}, std::move(layout.error)};
  return boundary_.read_observed_moment();
}
Outcome<WorldMoment>
NativeFieldApplicationRead::read_field_window_closing_moment() const {
  auto now = read_current_moment();
  if (!now.ok)
    return now;
  auto whole = compare_fractions(now.value.sub_half_day, CanonicalFraction{});
  if (!whole.ok)
    return {false, {}, std::move(whole.error)};
  if (whole.value != 0 || now.value.half_day_tick < world_.common.tick() ||
      __int128_t(now.value.half_day_tick) - world_.common.tick() > 1)
    return {false,
            {},
            common_failure("FieldWindowActualClosingBoundaryRequired").error};
  // read_observed_moment already proves an uncommitted closing via the actual
  // selected advance span/capture. The integer range above does not mint M.
  return now;
}
} // namespace hanmo::resident
