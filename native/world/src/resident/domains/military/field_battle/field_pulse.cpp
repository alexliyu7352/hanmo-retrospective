#include "field_event_legacy_kinds.hpp"
#include "field_pulse.hpp"
#include "resident/domains/military/battle_model.hpp"
#include "resident/domains/military/military_attack_damage_core.hpp"
#include "resident/domains/military/military_round_rng.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
bool trait(const FieldFormation &f, FieldTrait t) {
  return f.ability &&
         std::find(f.ability->traits.begin(), f.ability->traits.end(), t) !=
             f.ability->traits.end();
}
bool status(const FieldFormation &f, FieldStatusKind kind,
            const FieldBattleTime &now) {
  return std::any_of(f.statuses.begin(), f.statuses.end(), [&](const auto &s) {
    return s.kind == kind && (!s.until || before(now, *s.until));
  });
}
const FieldTacticActivation *active(const FieldFormation &f,
                                    FieldTacticKind kind,
                                    const FieldBattleTime &now) {
  if (!f.ability)
    return nullptr;
  for (const auto &a : f.ability->active)
    if (a.kind == kind &&
        (kind == FieldTacticKind::Volley ? a.remaining_pulses > 0
                                         : a.ends && before(now, *a.ends)))
      return &a;
  return nullptr;
}
const TroopDefinition *troop(const MilitaryBattleCatalog &c,
                             const StableId &id) {
  const TroopDefinition *found = nullptr;
  for (const auto &t : c.troop_types)
    if (t.id == id) {
      if (found)
        return nullptr;
      found = &t;
    }
  return found;
}
} // namespace
Outcome<FieldPulseRead> apply_field_casualty_pulse_in_candidate(
    FieldBattleSession &session, const GeneratedBattleField &field,
    const MilitaryBattleCatalog &catalog, const FieldBattleWindowSlice &step,
    CheckpointMemoryCharge &owner) {
  auto fail = [](const char *code) -> Outcome<FieldPulseRead> {
    return {false, {}, common_failure(code).error};
  };
  const auto &p = field.parameters;
  auto valid = validate_field_battle_parameters(p);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (session.state != FieldBattleState::Running ||
      session.field_hash != field.field_hash || step.steps != 1 ||
      step.from.tick != session.now.tick || step.from.t != session.now.t)
    return fail("FieldPulseBoundaryRequired");
  auto to = add_field_battle_seconds(step.from, p.step_s, p);
  if (!to.ok)
    return {false, {}, to.error};
  if (to.value.tick != step.to.tick || to.value.t != step.to.t)
    return fail("FieldPulseBoundaryRequired");
  FieldPulseRead out;
  const __int128_t end_seconds =
      __int128_t(step.to.tick) * p.window_s + step.to.t;
  if (end_seconds % p.pulse_s)
    return {true, std::move(out), {}};
  auto contacts = read_field_attack_contacts(session, field);
  if (!contacts.ok)
    return {false, {}, contacts.error};


  (owner).grow(0);
  // 报价由当前原名册冻结；所有攻击抽样完再统一扣名单，保持原 attack
  // 的同轮语义。
  std::map<StableId, Integer> damage;
  std::set<std::pair<StableId, StableId>> pairs;
  std::set<StableId> volley_used, charge_used, ambush_used;
  for (const auto &contact : contacts.value.attacks) {
    if (!pairs.emplace(contact.attacker_id, contact.target_id).second)
      return fail("FieldPulseContactRepeated");
    const auto a = session.formations.find(contact.attacker_id),
               b = session.formations.find(contact.target_id);
    if (a == session.formations.end() || b == session.formations.end() ||
        a->second.side == b->second.side || a->second.fit.empty() ||
        b->second.fit.empty() ||
        a->second.state == FieldFormationState::Routing)
      return fail("FieldPulseContactSourceRequired");
    const auto &attacker = a->second, &target = b->second;
    const auto *at = troop(catalog, attacker.troop_type_id),
               *dt = troop(catalog, target.troop_type_id);
    if (!at || !dt || attacker.fit.size() > 500 || target.fit.size() > 500 ||
        at->attack < 0 || at->attack > 100 || dt->defense < 0 ||
        dt->defense > 100 || at->accuracy < 0 || at->accuracy > 100)
      return fail("FieldPulseOriginalTroopRequired");
    const auto *charge = active(attacker, FieldTacticKind::Charge, session.now);
    const auto *volley = active(attacker, FieldTacticKind::Volley, session.now);
    FieldPulseImpact impact;
    impact.attacker_id = contact.attacker_id;
    impact.target_id = contact.target_id;
    Integer power = 10000;
    if (charge && !charge->first_contact_consumed && !contact.ranged) {
      power = charge->accepted_source.power_bp;
      charge_used.insert(charge->activation_id);
      impact.charge_activation = charge->activation_id;
      impact.morale_shock += charge->accepted_source.morale_effect;
    }
    if (volley && contact.ranged) {
      power = power * 15000 / 10000;
      volley_used.insert(volley->activation_id);
      impact.volley_activation = volley->activation_id;
      impact.suppressed = true;
    }
    if (trait(attacker, FieldTrait::Valiant) && !contact.ranged)
      power = power * 11000 / 10000;
    if (trait(attacker, FieldTrait::CavalryMaster) &&
        attacker.category == FieldTroopCategory::Cavalry &&
        status(attacker, FieldStatusKind::Charging, session.now))
      power = power * 11000 / 10000;
    for (const auto &[stratagem_id, stratagem] : session.stratagems)
      if (stratagem.kind == FieldStratagemKind::Ambush &&
          stratagem.success == true &&
          stratagem.formation_id == contact.attacker_id &&
          stratagem.revealed_at && !stratagem.first_contact_consumed) {
        if (impact.ambush_source)
          return fail("FieldAmbushSourceRepeated");
        impact.ambush_source = stratagem_id;
        impact.morale_shock += 15;
        power = power * 15000 / 10000;
        ambush_used.insert(stratagem_id);
      }
    auto base = original_military_attack_base(
        {at->accuracy, attacker.morale, Integer(attacker.fit.size()),
         at->attack, contact.facing, contact.cover,
         trait(attacker, FieldTrait::ArcheryMaster) && contact.ranged ? 10 : 0,
         false, contact.ranged, contact.target_forest, contact.target_hill,
         contact.height_advantage, false,
         status(attacker, FieldStatusKind::Suppressed, session.now), false,
         attacker.category == FieldTroopCategory::Cavalry &&
             contact.attacker_forest});
    OriginalMilitaryAttackDamageInput input;
    input.chance = base.chance;
    input.potential = base.potential;
    input.target_fit = target.fit.size();
    input.attacker_morale = attacker.morale;
    input.defense =
        dt->defense + (trait(target, FieldTrait::InfantryMaster) ? 10 : 0);
    input.guard = active(target, FieldTacticKind::Guard, session.now) ? 20 : 0;
    input.power_bp = power;
    input.facing = contact.facing;
    input.close_archer =
        !contact.ranged && attacker.category == FieldTroopCategory::Archer;
    input.ranged = contact.ranged;
    input.target_terrain = contact.target_forest ? "forest"
                           : contact.target_hill ? "hill"
                                                 : "plain";
    // 固定原 attack 规则 legacy_v1；不挑任意作者旧棋盘或采用未准的 dynamic2
    // 规则。
    auto quote = original_military_attack_damage(input);
    impact.flank = quote.flank;
    auto &remainder =
        session.pulse_remainders[{contact.attacker_id, contact.target_id}];
    if (remainder < 0 || remainder >= p.round_equiv_s)
      return fail("FieldPulseRemainderInvalid");
    const __int128_t numerator =
        __int128_t(quote.potential) * p.pulse_s + remainder;
    const auto potential = numerator / p.round_equiv_s;
    if (potential > 500)
      return fail("FieldPulsePotentialOverflow");
    remainder = static_cast<Integer>(numerator % p.round_equiv_s);
    auto draw =
        military_round_random_draw(session.rng_seed, session.rng_state, 1, 100);
    if (!draw.ok)
      return {false, {}, draw.error};
    const auto expectation = original_military_casualty_expectation(
        static_cast<Integer>(potential), quote.hit_chance);
    damage[contact.target_id] +=
        original_military_sampled_casualties(expectation, draw.value);
    out.impacts.push_back(std::move(impact));
  }
  for (const auto &[id, count] : damage) {
    auto &formation = session.formations.at(id);
    auto fit = formation.fit;
    std::sort(fit.begin(), fit.end());
    const auto actual = std::min<Integer>(count, fit.size());
    if (formation.casualty_ordinal < 0 ||
        formation.casualty_ordinal >
            std::numeric_limits<Integer>::max() - actual)
      return fail("FieldCasualtyOrdinalOverflow");
    FieldPulseLoss loss{id, Integer(fit.size()), 0, 0};
    for (Integer n = 0; n < actual; ++n) {
      formation.fit.erase(
          std::find(formation.fit.begin(), formation.fit.end(), fit[n]));
      if (original_military_casualty_is_wounded(formation.casualty_ordinal++,
                                                actual, false)) {
        formation.wounded.push_back(fit[n]);
        ++loss.wounded;
      } else {
        formation.dead.push_back(fit[n]);
        ++loss.dead;
      }
    }
    if (actual) {
      if (session.next_event_sequence <= 0 ||
          session.next_event_sequence == std::numeric_limits<Integer>::max())
        return fail("FieldEventSequenceExhausted");
      FieldBattleEvent event;
      event.sequence = session.next_event_sequence++;
      event.at = step.to;
      event.kind = field_event_kind::Casualties;
      event.formation_id = id;
      event.side = formation.side;
      event.count = actual;
      session.events.push_back(std::move(event));
      out.losses.push_back(std::move(loss));
    }
  }
  // 一次脉冲只消费一次战法计数，即使该编队有多个真实目标。
  for (auto &[id, formation] : session.formations)
    if (formation.ability)
      for (auto &activation : formation.ability->active) {
        if (activation.kind == FieldTacticKind::Volley &&
            volley_used.contains(activation.activation_id) &&
            activation.remaining_pulses > 0)
          --activation.remaining_pulses;
        if (activation.kind == FieldTacticKind::Charge &&
            charge_used.contains(activation.activation_id))
          activation.first_contact_consumed = true;
      }
  for (const auto &id : ambush_used)
    session.stratagems.at(id).first_contact_consumed = true;
  out.applied = true;
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
