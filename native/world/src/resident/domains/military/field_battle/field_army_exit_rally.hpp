// 终局后整军汇合只消费已发生的选择/终局/退出；不重开战斗或制造S0时刻。
#pragma once
#include "field_army_exit_selection.hpp"
#include "field_army_exit_model.hpp"
#include "field_segment_settlement.hpp"
namespace hanmo::resident {
// 原终局名册与位置的纯读投影；未入场余队不能凭空变成已经在场的编队。
struct FieldArmyExitRallyOriginRead {
  const FieldSegmentSettlement *terminal = nullptr;
  FieldBattleTime ordinary_deadline;
  std::vector<FieldArmyExitRallyFormation> formations;
};
Outcome<FieldArmyExitRallyOriginRead> read_field_army_exit_rally_origin(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const FieldArmyExitPlan &,
    const SourceRef &terminal_segment);
// 原terminal settlement封口后、仍在同一实际closing候选内初始化；不移动成员。
Status begin_field_army_exit_rallies_in_candidate(
    CommonStore &, FieldBattleWorldState &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &terminal_segment,
    CheckpointMemoryCharge &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 真实汇合历史的当前投影；普通兵已逃散不再同行，officer保留直到真ready采用。
struct FieldArmyExitFormationProgress {
  const FieldArmyExitRallyFormation *origin = nullptr;
  PointI position_dm;
  std::vector<StableId> ordinary_member_ids;
};
struct FieldArmyExitProgressRead {
  const FieldArmyExitPlan *plan = nullptr;
  FieldBattleTime at;
  std::vector<FieldArmyExitFormationProgress> formations;
};
// 逐来源头/原名册/几何重算所有已写步骤；返回值本身不是S1移动许可。
Outcome<FieldArmyExitProgressRead> read_field_army_exit_progress(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &choice);
// 同一个实际closing中每次只写一个有序本地步；false仅指该窗已到边界或全军ready。
Outcome<bool> advance_field_army_exit_rally_step_in_candidate(
    CommonStore &, FieldBattleWorldState &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const StableId &battle_id,
    CheckpointMemoryCharge &);
} // namespace hanmo::resident

namespace hanmo::resident {
struct FieldBattleMemberBinding;
// 真实新step父源，不冒原FieldSegmentSettlement；指针仅借同World未写入期。
struct FieldArmyExitStepSourceRead {
  const FieldArmyExitPlan *plan = nullptr;
  const FieldArmyExitStep *step = nullptr;
  const FieldBattleSession *session = nullptr;
};
Outcome<FieldArmyExitStepSourceRead> read_field_army_exit_step_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &step_source);
struct FieldArmyExitOrdinaryDepartureRead {
  FieldArmyExitStepSourceRead source;
  const FieldArmyExitMotion *motion = nullptr;
  const FieldFormation *formation = nullptr;
  // 非Common普通兵保真实roster身份而无actor binding；具名人物必须非空。
  const FieldBattleMemberBinding *member = nullptr;
};
// 只承认本真实deadline步的普通兵全名单；独立或兼任将领均不授逃散权限。
Outcome<FieldArmyExitOrdinaryDepartureRead> read_field_army_exit_ordinary_departure_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &step_source,
    const StableId &actor_id);
} // namespace hanmo::resident
