// 连续会战冷读与每次候选提交共用真实来源 validator；未知版本绝不当旧 profile。
#pragma once
#include "field_ability_sources.hpp"
#include "field_author_data.hpp"
#include "field_world.hpp"
namespace hanmo::resident {
// 历史任务与真实回执逐有限分支比较，不用今日任务替旧输入。
bool same_field_battle_mission(const FieldMission &, const FieldMission &);

class CommonStore;
// 只读取原 identity 中唯一明确模块，重复或未知版本拒绝。
Outcome<Integer> selected_field_battle_version(const CatalogIdentity &);
// 身份、不可变目录与可变根必须同一选择；缺一端显式拒绝，不视作旧 profile。
Status validate_field_battle_selection(const CatalogIdentity &,
                                       const FieldBattleCatalog *);
Status validate_field_battle_layout(const CatalogIdentity &,
                                    const FieldBattleCatalog *,
                                    const FieldBattleWorldState *);
// 命令和热/冷校验共用同一个任务约束；只读现有编队/阵营，不调整任务目标。
Status validate_field_battle_mission(const FieldMission &,
                                     const FieldFormation &,
                                     const FieldBattleSession &,
                                     const GeneratedBattleField &);
// 全部 Session/contact/binding 与真实 Common/军团/内容相互核对，再重建 hash。
Status validate_field_battle_world(
    const CommonStore &, const MilitaryState &, const TravelCatalog &,
    const FieldBattleCatalog &, const FieldBattleWorldState &,
    const FieldBattleAbilityReadContext *abilities = nullptr);
} // namespace hanmo::resident
