// The original attack multiplier/truncation leaf shared by legacy and exact S2.
#pragma once
#include "battle_model.hpp"
namespace hanmo::resident {
// 原 attack 的纯命中/基础伤亡输入；调用者先完成射界、地形、军令与来源校验。
// 不含 Hex、场景身份或 RNG，也不在这里夹限最终命中率。
struct OriginalMilitaryAttackBaseInput {
  Integer accuracy = 0, morale = 0, fit = 0, attack = 0, facing = 0, cover = 0,
          accuracy_bonus = 0;
  bool base_factors = false, ranged = false, target_forest = false,
       target_hill = false;
  bool height_advantage = false, inspired = false, suppressed = false,
       exposed = false, cavalry_in_forest = false;
};
struct OriginalMilitaryAttackBase {
  Integer chance = 0, potential = 0;
};
// 保留旧计算顺序与每次整数截断；只报价，不授予攻击资格或消耗随机数。
OriginalMilitaryAttackBase
original_military_attack_base(const OriginalMilitaryAttackBaseInput &) noexcept;
struct OriginalMilitaryAttackDamageInput {
  Integer chance = 0, potential = 0, target_fit = 0, attacker_morale = 0,
          defense = 0, guard = 0, power_bp = 10000, incoming_bp = 10000,
          facing = 0;
  bool close_archer = false, ranged = false, commanded = false,
       learned_guard = false;
  StableId target_terrain;
  MilitaryDamageModel damage_model;
  std::optional<Integer> ranged_wall_bp;
  Integer fatigue_bp = 10000;
};
// All qualification and original catalog validation belongs to the caller. This
// pure leaf preserves the original formula and does not grant an attack or
// draw.
MilitaryAttackQuote
original_military_attack_damage(const OriginalMilitaryAttackDamageInput &);
// Original bounded damage arithmetic only. Callers retain their own complete
// qualification, actual residual member pool, random owner and effect timing.
// Validated quotes have potential in [0,500], chance in [0,100]. A legacy
// caller still consumes its one actual 1..100 draw even when remainder is 0;
// commanded damage uses minimum without creating a random draw.
struct OriginalMilitaryCasualtyExpectation {
  Integer minimum = 0, maximum = 0, remainder = 0;
};
OriginalMilitaryCasualtyExpectation
original_military_casualty_expectation(Integer potential,
                                       Integer chance) noexcept;
Integer original_military_sampled_casualties(
    const OriginalMilitaryCasualtyExpectation &, Integer actual_draw) noexcept;
// These functions do not allocate/select members or consume a support token.
// Original counts/fit are bounded by 500; ordinal is in [0,actual_count).
Integer original_military_support_share(Integer actual_count,
                                        Integer supporter_remaining) noexcept;
bool original_military_casualty_is_wounded(Integer ordinal,
                                           Integer actual_count,
                                           bool commanded) noexcept;
// A caller with a positive original attack may have direct_count == 0 after
// sharing. Preserve the original commanded +2 and legacy flank in that case;
// the original owner decides whether this loss is applicable at all.
Integer
original_military_casualty_morale_loss(Integer direct_count,
                                       Integer snapshot_fit, bool commanded,
                                       Integer original_flank = 0) noexcept;
// Only the original non-enlisted exposure arithmetic. The caller proves the
// actual unit-local killed/wounded delta, positive snapshot_fit, original
// 0..10000 BP policy and actual 1..10000 draw. Enlisted commanders do not call
// this leaf or consume another draw; land/naval cause production stays local.
Integer original_military_commander_exposure_loss(Integer actual_killed,
                                                  Integer actual_wounded,
                                                  Integer snapshot_fit,
                                                  Integer exposure_bp,
                                                  Integer actual_draw) noexcept;

} // namespace hanmo::resident
