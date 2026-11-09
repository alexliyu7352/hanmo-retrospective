// 原未选择证明与明选永久伤效的日历只读闭包；不提供疗程或恢复 writer。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct CharacterConditionSources;
// body 必须借同 World 的完整原 owning 图，并在本次调用期间保持存活。
Status
validate_native_condition_due_absence(const WorldBundle &,
                                      const CharacterConditionSources &body);
// 只接 Condition8/source1/map3 的原永久伤效：真身体全链、同世界时刻与
// 原producer反覆盖必须成立；普通rest接原Time2/source1，未接到期、长休、治疗仍拒绝。
Status validate_native_permanent_injury_calendar_sources(
    const WorldBundle &, const CharacterConditionSources &body);
// 只允许原个人恢复模块、目录、seed 和所有永久来源确实未选择。
Status validate_native_growth_recovery_absence(const WorldBundle &);
} // namespace hanmo::resident
