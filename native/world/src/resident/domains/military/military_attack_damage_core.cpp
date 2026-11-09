#include "military_attack_damage_core.hpp"
#include <algorithm>
#include <numeric>
namespace hanmo::resident {
namespace {
Integer scaled_floor(Integer base, Integer numerator, Integer denominator) {
  Integer result = 0, remainder = 0;
  for (Integer bit = 4096; bit > 0; bit >>= 1) {
    auto current = remainder * 2 + ((base & bit) ? numerator : 0);
    result = result * 2 + current / denominator;
    remainder = current % denominator;
  }
  return result;
}
} // namespace
// 由原 battle_rules::attack 机械提取；旧、新战场共同调用唯一数值叶。
OriginalMilitaryAttackBase original_military_attack_base(
    const OriginalMilitaryAttackBaseInput &in) noexcept {
  Integer cover = in.cover;
  if (in.base_factors &&
      ((in.ranged && in.target_forest) || (!in.ranged && in.target_hill)))
    cover = 0;
  Integer chance = in.accuracy + (in.morale - 50) / 10 +
                   (in.base_factors  ? 0
                    : in.facing == 3 ? 20
                    : in.facing == 2 ? 10
                                     : 0) -
                   cover + in.accuracy_bonus;
  if (in.height_advantage)
    chance += 10;
  if (in.inspired)
    chance += 10;
  if (in.suppressed)
    chance -= 15;
  if (in.exposed)
    chance += 15;
  if (in.cavalry_in_forest)
    chance -= 15;
  return {chance, std::max<Integer>(1, (in.fit * in.attack + 99) / 100)};
}
MilitaryAttackQuote
original_military_attack_damage(const OriginalMilitaryAttackDamageInput &in) {
  Integer chance = in.chance, potential = in.potential;
  const bool candidate = in.damage_model.kind == "legacy_base_factors_v1";
  if (!candidate) {
    potential =
        std::max<Integer>(1, potential * (50 + in.attacker_morale / 2) / 100);
    potential = std::max<Integer>(1, potential * in.power_bp / 10000);
    if (in.close_archer)
      potential = std::max<Integer>(1, potential * 5000 / 10000);
    potential = std::max<Integer>(
        1,
        potential * (100 - std::min<Integer>(95, in.defense + in.guard)) / 100);
    potential = std::max<Integer>(1, potential * in.incoming_bp / 10000);
  } else {
    if (!in.commanded)
      potential = std::max<Integer>(
          1, potential * (100 - std::min<Integer>(95, in.defense + in.guard)) /
                 100);
    const auto &m = in.damage_model;
    const auto *wall = in.ranged_wall_bp ? &*in.ranged_wall_bp : nullptr;
    Integer numerator = 1, denominator = 1;
    for (Integer factor :
         {5000 + 50 * in.attacker_morale, in.power_bp,
          in.close_archer ? Integer(5000) : Integer(10000), in.incoming_bp,
          in.facing == 3   ? m.rear_bp
          : in.facing == 2 ? m.side_bp
                           : 10000,
          in.ranged && in.target_terrain == "forest"  ? m.forest_ranged_bp
          : !in.ranged && in.target_terrain == "hill" ? m.hill_melee_bp
                                                      : 10000,
          in.ranged && wall ? *wall : 10000, in.fatigue_bp,
          in.commanded && in.guard > 0 && !(in.learned_guard)
              ? Integer(8000)
              : Integer(10000)}) {
      Integer top = factor, bottom = 10000, d = std::gcd(top, bottom);
      top /= d;
      bottom /= d;
      d = std::gcd(numerator, bottom);
      numerator /= d;
      bottom /= d;
      d = std::gcd(denominator, top);
      denominator /= d;
      top /= d;
      numerator *= top;
      denominator *= bottom;
    }
    if (numerator * 5 < denominator * 2) {
      numerator = m.min_total_bp;
      denominator = 10000;
    } else if (numerator * 5 > denominator * 9) {
      numerator = m.max_total_bp;
      denominator = 10000;
    }
    potential =
        std::max<Integer>(1, scaled_floor(potential, numerator, denominator));
  }
  return {std::clamp<Integer>(chance, 0, 100),
          std::min<Integer>(potential, in.target_fit),
          in.facing == 3   ? 10
          : in.facing == 2 ? 5
                           : 0};
}
OriginalMilitaryCasualtyExpectation
original_military_casualty_expectation(Integer potential,
                                       Integer chance) noexcept {
  const Integer weighted = potential * chance;
  return {weighted / 100, (weighted + 99) / 100, weighted % 100};
}
Integer original_military_sampled_casualties(
    const OriginalMilitaryCasualtyExpectation &expectation,
    Integer draw) noexcept {
  return expectation.minimum + (draw <= expectation.remainder ? 1 : 0);
}
Integer original_military_support_share(Integer count,
                                        Integer remaining) noexcept {
  return std::min(remaining, count * 2000 / 10000);
}
bool original_military_casualty_is_wounded(Integer ordinal, Integer count,
                                           bool commanded) noexcept {
  return commanded ? ordinal < count * 60 / 100 : ordinal % 3 != 0;
}
Integer original_military_casualty_morale_loss(Integer count, Integer fit,
                                               bool commanded,
                                               Integer flank) noexcept {
  const Integer proportional = count * 100 / std::max<Integer>(1, fit);
  return commanded ? std::min<Integer>(20, 2 + proportional)
                   : proportional + flank;
}

Integer original_military_commander_exposure_loss(Integer killed,
                                                  Integer wounded, Integer fit,
                                                  Integer exposure,
                                                  Integer draw) noexcept {
  const Integer death_bp = killed * exposure / fit,
                hurt_bp = (killed + wounded) * exposure / fit;
  return draw <= death_bp ? 2 : draw <= hurt_bp ? 1 : 0;
}

} // namespace hanmo::resident
