#include "field_binding_source.hpp"
#include "field_contact_origin.hpp"
#include "field_reinforcement_queue.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<const FieldBattleRoadBinding *> read_field_original_battle_binding(
    const CommonStore &common, const FieldBattleWorldState &world,
    const StableId &battle, const StableId &army) {
  auto fail = [&]() -> Outcome<const FieldBattleRoadBinding *> {
    return {false, nullptr,
            common_failure("FieldOriginalBindingSourceMismatch", army).error};
  };
  const auto session = world.sessions.find(battle);
  if (session == world.sessions.end())
    return fail();
  auto session_head = read_actual_common_fact_position(
      common, SourceRef{"military", "field_battle_session", battle});
  if (!session_head.ok)
    return {false, nullptr, session_head.error};
  const FieldBattleRoadBinding *selected = nullptr;
  for (const auto &[id, contact] : world.contacts) {
    if (contact.battle_id != battle)
      continue;
    if (id != contact.contact_id)
      return fail();
    auto origin = validate_field_contact_origin_head(common, contact);
    if (!origin.ok)
      return {false, nullptr, origin.error};
    auto head = read_actual_common_fact_position(
        common, SourceRef{"military", "field_battle_contact", id});
    if (!head.ok)
      return {false, nullptr, head.error};
    if (head.value >= session_head.value)
      return fail();
    for (Integer i = 0; i < 2; ++i) {
      const auto &binding = contact.original_bindings[i];
      const auto initial =
          std::get_if<FieldInitialContactBinding>(&binding.source);
      if (!initial || initial->contact_id != id ||
          initial->source_endpoint != i || binding.army_id.empty() ||
          binding.battle_id != battle || binding.commander_actor_id.empty() ||
          contact.original_bindings[1 - i].army_id == binding.army_id)
        return fail();
      if (const auto legacy = std::get_if<FieldRoadContact>(&contact.source)) {
        const auto &motion =
            i == 0 ? legacy->contact.road->first : legacy->contact.road->second;
        if (binding.army_id != legacy->army_ids[i] ||
            binding.action_id != motion.trace.action_id ||
            binding.commander_actor_id != motion.actor_id ||
            motion.actor_id != motion.trace.actor_id)
          return fail();
      } else {
        // Exact group execution has no legacy per-commander action ID. The
        // original S1 journey/order is validated with the whole contact source.
        if (!binding.action_id.empty())
          return fail();
      }
      if (binding.army_id != army)
        continue;
      if (selected)
        return fail();
      selected = &binding;
    }
  }
  for (const auto &[id, entry] : world.boundary_contacts) {
    if (entry.battle_id != battle || entry.army_id != army)
      continue;
    if (!entry.entered_binding) {
      if (entry.entered_at || entry.entry_application_source)
        return fail();
      continue;
    }
    const auto &binding = *entry.entered_binding;
    const auto boundary =
        std::get_if<FieldBoundaryContactBinding>(&binding.source);
    if (selected || entry.contact_id != id || !entry.entered_at ||
        !entry.entry_application_source || !entry.entered_actual_at ||
        !entry.entered_observed_tick || !boundary ||
        boundary->contact_id != id || binding.army_id != army ||
        binding.battle_id != battle ||
        entry.entry_application_source->domain != "military" ||
        entry.entry_application_source->kind != "field_battle_entered")
      return fail();
    auto contact_head = read_actual_common_fact_position(
        common, SourceRef{"military", "field_battle_contact", id});
    auto entry_head = read_actual_common_fact_position(
        common, *entry.entry_application_source);
    if (!contact_head.ok)
      return {false, nullptr, contact_head.error};
    if (!entry_head.ok)
      return {false, nullptr, entry_head.error};
    if (contact_head.value >= entry_head.value ||
        session_head.value >= entry_head.value)
      return fail();
    bool motion_matches = false;
    std::visit(
        [&](const auto &motion) {
          using T = std::decay_t<decltype(motion)>;
          if constexpr (std::is_same_v<T, FieldExactBoundaryOrigin>) {
            // Identity only. The full Field boundary validator separately
            // proves this single-prefix source's original group/order/members.
            const auto *head =
                common.state().facts.find(motion.actual_contact.id);
            motion_matches =
                binding.action_id.empty() && head &&
                motion.actual_contact.domain == "travel" &&
                motion.actual_contact.kind == "movement_boundary_contact" &&
                head->domain == motion.actual_contact.domain &&
                head->kind == motion.actual_contact.kind;
          } else {
            motion_matches = binding.action_id == motion.trace.action_id &&
                             binding.commander_actor_id == motion.actor_id &&
                             motion.actor_id == motion.trace.actor_id;
          }
        },
        entry.motion);
    if (!motion_matches)
      return fail();
    // The entry binding is the append-only union of actual admitted units,
    // not the complete queued Army. Later admissions own different heads/M.
    if (!entry.reinforcement_queue ||
        entry.reinforcement_queue->admitted.empty())
      return fail();
    std::size_t expected_members = 0;
    for (const auto &actor : entry.participant_actor_ids) {
      const FieldFormation *admitted_formation = nullptr;
      for (const auto &admission : entry.reinforcement_queue->admitted) {
        const auto found =
            session->second.formations.find(admission.formation_id);
        if (found == session->second.formations.end())
          return fail();
        const auto &formation = found->second;
        auto actual = read_field_reinforcement_admission_identity(
            common, world, session->second, formation);
        if (!actual.ok)
          return {false, nullptr, actual.error};
        if (actual.value != &admission)
          return fail();
        bool member = formation.commander_actor_id == actor;
        for (const auto *group :
             {&formation.fit, &formation.wounded, &formation.dead,
              &formation.captured, &formation.deserted})
          member = member || std::find(group->begin(), group->end(), actor) !=
                                 group->end();
        if (!member)
          continue;
        if (admitted_formation)
          return fail();
        admitted_formation = &formation;
      }
      if (!admitted_formation)
        continue; // still queued; original queue roster coverage is separate
      ++expected_members;
      if (std::count_if(binding.members.begin(), binding.members.end(),
                        [&](const auto &member) {
                          return member.actor_id == actor &&
                                 member.formation_id ==
                                     admitted_formation->formation_id;
                        }) != 1)
        return fail();
    }
    if (expected_members != binding.members.size())
      return fail();
    selected = &binding;
  }
  if (!selected || selected->members.empty())
    return fail();
  for (const auto &member : selected->members) {
    if (member.actor_id.empty() || !common.actor(member.actor_id) ||
        std::count_if(
            selected->members.begin(), selected->members.end(),
            [&](const auto &v) { return v.actor_id == member.actor_id; }) != 1)
      return fail();
    const auto formation = session->second.formations.find(member.formation_id);
    if (formation == session->second.formations.end() ||
        formation->second.army_id != army)
      return fail();
  }
  return {true, selected, {}};
}
} // namespace hanmo::resident
