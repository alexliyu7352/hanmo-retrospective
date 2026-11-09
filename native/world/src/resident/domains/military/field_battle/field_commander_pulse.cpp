#include "field_event_legacy_kinds.hpp"
#include "field_commander_pulse.hpp"
#include "resident/domains/military/military_attack_damage_core.hpp"
#include "resident/domains/military/military_round_rng.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
Outcome<FieldCommanderPulseRead> apply_field_commander_pulse_in_candidate(
    FieldBattleSession &session, const GeneratedBattleField &field,
    const FieldPulseRead &pulse, const FieldBattleWindowSlice &step,
    CheckpointMemoryCharge &owner) {
  auto fail = [](const char *code) -> Outcome<FieldCommanderPulseRead> {
    return {false, {}, common_failure(code).error};
  };
  FieldCommanderPulseRead out;
  if (!pulse.applied)
    return {true, std::move(out), {}};
  if (session.field_hash != field.field_hash ||
      step.from.tick != session.now.tick || step.from.t != session.now.t)
    return fail("FieldCommanderPulseBoundaryMismatch");


  (owner).grow(0);
  std::set<StableId> consumed;
  for (const auto &loss : pulse.losses) {
    auto found = session.formations.find(loss.formation_id);
    if (found == session.formations.end() ||
        !consumed.insert(loss.formation_id).second || loss.fit_before <= 0 ||
        loss.wounded < 0 || loss.dead < 0 ||
        __int128_t(loss.wounded) + loss.dead > loss.fit_before)
      return fail("FieldCommanderPulseSourceRequired");
    if (loss.wounded + loss.dead == 0)
      continue;
    auto &f = found->second;
    const StableId *actor = &f.commander_actor_id;
    if (f.commander_lost_at) {
      if (!f.deputy_actor_id)
        continue;
      actor = &*f.deputy_actor_id;
    }
    Integer previous_loss = 0;
    for (const auto &v : f.commander_casualties)
      if (v.actor_id == *actor) {
        if (v.loss <= previous_loss || v.loss < 1 || v.loss > 2)
          return fail("FieldCommanderLossHistoryInvalid");
        previous_loss = v.loss;
      }
    if (previous_loss == 2)
      continue;
    Integer result = 0;
    const auto member = [&](const auto &list) {
      return std::find(list.begin(), list.end(), *actor) != list.end();
    };
    const bool enlisted = member(f.fit) || member(f.wounded) ||
                          member(f.dead) || member(f.captured) ||
                          member(f.deserted);
    if (enlisted) {
      // 入伍将领跟随自己的真实成员结果，不额外再抽一次暴露。
      if (member(f.dead))
        result = 2;
      else if (member(f.wounded))
        result = 1;
    } else {
      Integer exposure = field.parameters.commander_exposure_bp;
      if (f.ability && f.ability->commander_actor_id == *actor &&
          std::find(f.ability->traits.begin(), f.ability->traits.end(),
                    FieldTrait::Valiant) != f.ability->traits.end())
        exposure /= 2;
      if (exposure < 0 || exposure > 10000)
        return fail("FieldCommanderExposureInvalid");
      auto draw = military_round_random_draw(session.rng_seed,
                                             session.rng_state, 1, 10000);
      if (!draw.ok)
        return {false, {}, draw.error};
      result = original_military_commander_exposure_loss(
          loss.dead, loss.wounded, loss.fit_before, exposure, draw.value);
    }
    if (!result || previous_loss >= result)
      continue;
    // Original segment cold sources require the earlier wounded observation
    // even after a later fatal pulse. Append the monotone transition; never
    // overwrite the wound's actual local boundary with the later death.
    f.commander_casualties.push_back({*actor, result, step.to});
    out.changed_formations.push_back(f.formation_id);
    if (result == 2) {
      if (*actor == f.commander_actor_id)
        f.commander_lost_at = step.to;
      f.morale = std::max<Integer>(0, f.morale - 20);
      out.lost_formations.push_back(f.formation_id);
      if (session.next_event_sequence <= 0 ||
          session.next_event_sequence == std::numeric_limits<Integer>::max())
        return fail("FieldEventSequenceExhausted");
      FieldBattleEvent event;
      event.sequence = session.next_event_sequence++;
      event.at = step.to;
      event.kind = field_event_kind::CommanderLost;
      event.formation_id = f.formation_id;
      event.side = f.side;
      event.count = 2;
      session.events.push_back(std::move(event));

      auto &triggers = session.ai[f.side].pending_triggers;
      if (std::find(triggers.begin(), triggers.end(),
                    FieldAiTrigger::CommanderLost) == triggers.end()) {
        triggers.push_back(FieldAiTrigger::CommanderLost);
        std::sort(triggers.begin(), triggers.end());
      }
    }
  }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
