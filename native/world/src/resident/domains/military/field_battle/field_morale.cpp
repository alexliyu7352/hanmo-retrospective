#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "field_morale.hpp"
#include "field_ability_sources.hpp"
#include "field_integer_geometry.hpp"
#include "field_mission_transition.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool inspiring(const FieldFormation &f) {
  return f.ability &&
         std::find(f.ability->traits.begin(), f.ability->traits.end(),
                   FieldTrait::Inspiring) != f.ability->traits.end();
}
bool present(const FieldFormation &f) {
  return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
         f.state != FieldFormationState::Destroyed &&
         f.state != FieldFormationState::Deploying;
}
} // namespace
Outcome<FieldMoraleRead> apply_field_pulse_morale_in_candidate(
    const CommonStore &common, FieldBattleSession &session,
    const GeneratedBattleField &field, const FieldPulseRead &pulse,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  auto fail = [](const char *code) -> Outcome<FieldMoraleRead> {
    return {false, {}, common_failure(code).error};
  };
  const auto &p = field.parameters;
  FieldMoraleRead out;
  if (!pulse.applied)
    return {true, std::move(out), {}};
  if (session.field_hash != field.field_hash ||
      step.from.tick != session.now.tick || step.from.t != session.now.t)
    return fail("FieldMoraleBoundaryMismatch");


  (owner).grow(0);
  std::map<StableId, Integer> casualties, flanks, shocks;
  for (const auto &loss : pulse.losses) {
    if (loss.wounded < 0 || loss.dead < 0 ||
        loss.wounded > std::numeric_limits<Integer>::max() - loss.dead ||
        !casualties.emplace(loss.formation_id, loss.wounded + loss.dead).second)
      return fail("FieldMoraleCasualtySourceRepeated");
  }
  for (const auto &impact : pulse.impacts) {
    if (!session.formations.contains(impact.attacker_id) ||
        !session.formations.contains(impact.target_id) || impact.flank < 0 ||
        impact.flank > 10 || impact.morale_shock < 0)
      return fail("FieldMoraleImpactSourceRequired");
    if (flanks[impact.target_id] >
            std::numeric_limits<Integer>::max() - impact.flank ||
        shocks[impact.target_id] >
            std::numeric_limits<Integer>::max() - impact.morale_shock)
      return fail("FieldMoraleImpactOverflow");
    flanks[impact.target_id] += impact.flank;
    shocks[impact.target_id] += impact.morale_shock;
    if (impact.suppressed) {
      if (!impact.volley_activation)
        return fail("FieldVolleyActivationRequired");
      auto until = add_field_battle_seconds(step.to, 600, p);
      if (!until.ok)
        return {false, {}, until.error};
      if (session.next_event_sequence <= 0 ||
          session.next_event_sequence == std::numeric_limits<Integer>::max())
        return fail("FieldEventSequenceExhausted");
      FieldBattleEvent hit;
      hit.sequence = session.next_event_sequence++;
      hit.at = step.to;
      hit.kind = field_event_kind::VolleyImpact;
      hit.formation_id = impact.attacker_id;
      hit.target_formation_id = impact.target_id;
      hit.side = session.formations.at(impact.attacker_id).side;
      hit.source_id = *impact.volley_activation;
      hit.tactic = FieldTacticKind::Volley;
      session.events.push_back(std::move(hit));
      auto &statuses = session.formations.at(impact.target_id).statuses;
      auto existing =
          std::find_if(statuses.begin(), statuses.end(), [&](const auto &s) {
            return s.kind == FieldStatusKind::Suppressed &&
                   s.source_id == *impact.volley_activation;
          });
      if (existing == statuses.end())
        statuses.push_back({FieldStatusKind::Suppressed,
                            *impact.volley_activation, until.value});
      else if (!existing->until ||
               std::tie(existing->until->tick, existing->until->t) <
                   std::tie(until.value.tick, until.value.t))
        existing->until = until.value;
    }
  }
  for (auto &[id, f] : session.formations) {
    if (f.fit_start <= 0 || f.morale < 0 || f.morale > 100)
      return fail("FieldMoraleFormationInvalid");
    const Integer inspire_bp = inspiring(f) ? 8500 : 10000;
    // 固定分母保留比例小数；开战被动按原合同冻结。
    const __int128_t denominator = __int128_t(f.fit_start) * 10000 * 10000;
    if (denominator > std::numeric_limits<Integer>::max())
      return fail("FieldMoraleRemainderOverflow");
    auto &remainder = session.morale_remainders[id];
    if (remainder < 0 || remainder >= denominator)
      return fail("FieldMoraleRemainderInvalid");
    const __int128_t numerator =
        __int128_t(casualties[id]) * 100 * p.morale_loss_bp * inspire_bp +
        remainder;
    __int128_t loss = numerator / denominator;
    remainder = static_cast<Integer>(numerator % denominator);
    auto &flank_remainder = session.flank_remainders[id];
    if (flank_remainder < 0 || flank_remainder >= p.round_equiv_s)
      return fail("FieldFlankRemainderInvalid");
    const __int128_t flank =
        __int128_t(flanks[id]) * p.pulse_s + flank_remainder;
    loss += flank / p.round_equiv_s + shocks[id];
    flank_remainder = static_cast<Integer>(flank % p.round_equiv_s);
    f.morale = static_cast<Integer>(
        std::max<__int128_t>(0, __int128_t(f.morale) - loss));
    if (!present(f))
      continue;
    if (f.state == FieldFormationState::Routing) {
      bool far = true;
      for (const auto &[other_id, other] : session.formations)
        if (other.side != f.side && present(other) &&
            field_distance_squared(f.position_dm, other.position_dm) <=
                __int128_t(6000) * 6000) {
          far = false;
          break;
        }
      auto commander = read_field_effective_commander(common, f);
      if (!commander.ok &&
          commander.error.original.code != "FieldTacticNoCommander")
        return {false, {}, commander.error};
      if (far && commander.ok)
        f.morale = std::min<Integer>(100, f.morale + 2);
      if (f.morale >= 30) {
        if (session.next_event_sequence <= 0 ||
            session.next_event_sequence == std::numeric_limits<Integer>::max())
          return fail("FieldEventSequenceExhausted");
        FieldBattleEvent recovered;
        recovered.sequence = session.next_event_sequence++;
        recovered.at = step.to;
        recovered.kind = field_event_kind::Rallied;
        recovered.formation_id = id;
        recovered.side = f.side;
        const auto rallied_sequence = recovered.sequence;
        session.events.push_back(std::move(recovered));
        auto changed = apply_field_rule_mission_transition(
            session, f, field, FieldMissionTransitionReason::Rallied, step.to,
            owner, rallied_sequence);
        if (!changed.ok)
          return {false, {}, changed.error};
        f.state = FieldFormationState::Holding;
        f.hold_since = step.to;
        f.ai_pursuit = false;
        f.followup_mission.reset();
        f.wave_mission.reset();
        f.navigation_target_dm.reset();
        f.navigation_observation_sequence.reset();
        f.navigation_replanned_at.reset();
        out.rallied.push_back(id);
      }
    } else if (f.state != FieldFormationState::Engaged) {
      f.morale =
          std::min<Integer>(f.morale_start, f.morale + (inspiring(f) ? 2 : 1));
    }
    if (f.morale <= 0)
      out.zero_morale.push_back(id);
  }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
