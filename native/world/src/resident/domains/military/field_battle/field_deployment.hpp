// AI 初始布局的纯值计划；还未决定的统帅任务不在此叶猜定。
#pragma once
#include "field_session.hpp"
#include <set>
namespace hanmo::resident {
enum class FieldAiStrategy { HoldGround, LineReserve, Flanking };
struct FieldDeploymentPlacement {
  StableId formation_id;
  PointI point_dm;
  Integer facing_mdeg = 0;
  bool reserve = false, commander = false;
  Integer wing = 0; // original layout across sign; zero is exactly centered
};
struct FieldDeploymentLayout {
  FieldAiStrategy strategy = FieldAiStrategy::LineReserve;
  std::vector<FieldDeploymentPlacement> placements;
};
// Geometry-only original side binding. Exits are separately qualified by P3;
// they must not be compared to the unqualified generated boundary cells.
Outcome<const FieldSideZone *>
read_field_side_geometry(const FieldBattleSession &,
                         const GeneratedBattleField &, Integer side);
// 从真实会话名册和已生成布阵格排位，后排按合法格稳定选择，不种假单位。
Outcome<FieldDeploymentLayout>
prepare_field_deployment_layout(const FieldBattleSession &,
                                const GeneratedBattleField &, Integer side);
// 军议定案可明确选择原三种布局；只摆位，不替任务或波次另作决定。
Outcome<FieldDeploymentLayout>
prepare_field_deployment_layout(const FieldBattleSession &,
                                const GeneratedBattleField &, Integer side,
                                FieldAiStrategy adopted_strategy);
// 占地只取合同 ranks 与 fit；外接圆向上取整，碰撞不因截断缩小。
Integer field_formation_width_dm(const FieldFormation &);
Integer field_formation_radius_dm(const FieldFormation &);
// Same original geometry for a verified historical fit-prefix count.
Integer field_formation_radius_dm(const FieldFormation &,
                                  Integer original_fit_count);
// Same layout algorithm, frozen initial contact membership and verified
// original fit_start instead of today's survivors. No second location state.
Outcome<FieldDeploymentLayout> prepare_field_original_deployment_layout(
    const FieldBattleSession &, const GeneratedBattleField &, Integer side,
    FieldAiStrategy, const std::set<StableId> &original_formation_ids);
} // namespace hanmo::resident
