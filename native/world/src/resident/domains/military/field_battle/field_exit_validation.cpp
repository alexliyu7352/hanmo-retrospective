#include "field_exit_validation.hpp"
#include "field_ability_sources.hpp"
#include "field_active_membership.hpp"
#include "field_exact_contact_source.hpp"
#include "field_segment_settlement.hpp"
#include "field_settlement_exit_source.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_field_original_exit_handoffs(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext *abilities) {
  for (const auto &[id, fact] : common.state().facts.rows())
    if (fact.domain == "military" && fact.kind == "field_battle_exit" &&
        (fact.id != id || !world.exits.contains(id)))
      return common_failure("FieldExitOriginalCoverageMissing", id);
  if (world.exits.empty())
    return common_success();
  if (!abilities || !abilities->exact_contacts ||
      !abilities->active_membership ||
      !abilities->active_membership->bound_to(common, world))
    return common_failure("FieldExitOriginalHandoffOwnerRequired");
  const auto &x = *abilities->exact_contacts;
  if (&x.common != &common || &x.military != &military || &x.field != &world ||
      &x.travel != &travel || !x.clock.bound_to(common, military, world))
    return common_failure("CrossCandidateFieldExitHandoff");
  for (const auto &[id, exit] : world.exits) {
    if (id != exit.fact_id)
      return common_failure("FieldExitOriginalSourceMismatch", id);
    const SourceRef ref{"military", "field_battle_exit", id};
    auto source = read_field_actual_exit_application(
        common, military, world, travel, catalog, ref, x.clock, {}, &x,
        abilities->diplomacy, abilities->territory);
    if (!source.ok)
      return {false, {}, source.error};
    if (source.value != &exit || exit.named_actor_ids.empty())
      return common_failure("FieldExitOriginalSourceMismatch", id);
    const GroupContactMemberReleaseSource *same_release = nullptr;
    for (const auto &actor : exit.named_actor_ids) {
      auto engagement =
          abilities->active_membership->read_field_active_member_engagement(
              exit.battle_id, actor);
      if (!engagement.ok)
        return {false, {}, engagement.error};
      const auto &entry = engagement.value;
      const auto *released =
          std::get_if<const GroupContactMemberReleaseSource *>(&entry.release);
      const auto *release = released ? *released : nullptr;

      if (!entry.battle_id || *entry.battle_id != exit.battle_id ||
          !has_field_engagement_borrowed_entry(entry.entry) || !release ||
          !entry.journey_id || release->journey_id != *entry.journey_id ||
          release->release_schema != 1 ||
          !same_source_ref(release->actual_release_source, ref) ||
          std::count(release->named_participants.begin(),
                     release->named_participants.end(), actor) != 1 ||
          release->named_participants != exit.named_actor_ids ||
          (same_release && same_release != release))
        return common_failure("FieldExitActualMemberHandoffRequired", actor);
      auto actual_m =
          compare_world_moments(release->occurred_at, exit.applied_at);
      if (!actual_m.ok)
        return {false, {}, actual_m.error};
      if (actual_m.value != 0)
        return common_failure("FieldExitHandoffMomentMismatch", id);
      auto exit_head = read_actual_common_fact_position(common, ref);
      auto release_head = read_actual_common_fact_position(
          common, {"travel", "group_contact_member_release", release->id});
      if (!exit_head.ok)
        return {false, {}, exit_head.error};
      if (!release_head.ok)
        return {false, {}, release_head.error};
      if (exit_head.value >= release_head.value)
        return common_failure("FieldExitHandoffOrderMismatch", id);
      same_release = release;
    }
  }
  return common_success();
}
} // namespace hanmo::resident
