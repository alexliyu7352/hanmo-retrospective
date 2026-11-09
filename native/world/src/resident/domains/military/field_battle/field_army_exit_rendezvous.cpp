#include "field_army_exit_rendezvous.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <limits>
#include <tuple>
namespace hanmo::resident {
// 终局后只建立本步同一当前编队几何投影；不将局部秒换成世界M，也不重开战斗脉冲。
Outcome<FieldNavigationStep> read_field_post_result_exit_step(
    const FieldBattleSession &session, const GeneratedBattleField &field,
    const StableId &formation_id, std::size_t selected_exit,
    const FieldBattleWindowSlice &step) {
  auto fail = [&](const char *code) -> Outcome<FieldNavigationStep> {
    return {false, {}, common_failure(code, formation_id).error};
  };
  if (session.state != FieldBattleState::Ended || !session.result ||
      session.field_hash != field.field_hash || step.steps != 1 ||
      step.from.tick != session.now.tick || step.from.t != session.now.t ||
      std::tie(step.from.tick, step.from.t) <
          std::tie(session.result->ended.tick, session.result->ended.t))
    return fail("FieldArmyExitActualPostResultStepRequired");
  auto parameters = validate_field_battle_parameters(field.parameters);
  if (!parameters.ok)
    return {false, {}, parameters.error};
  const auto next = add_field_battle_seconds(step.from, field.parameters.step_s,
                                             field.parameters);
  if (!next.ok)
    return {false, {}, next.error};
  if (next.value.tick != step.to.tick || next.value.t != step.to.t)
    return fail("FieldMotionActualStepRequired");
  if (field.columns <= 0 || field.rows <= 0 ||
      field.columns != field.parameters.length_m / field.parameters.cell_m ||
      field.rows != field.parameters.width_m / field.parameters.cell_m ||
      __int128_t(field.columns) * field.rows != field.cells.size())
    return fail("FieldArmyExitOriginalGeometryRequired");
  if (session.formations.size() >
      std::size_t(field.parameters.max_formations_per_side) * 2)
    return fail("FieldArmyExitFormationBoundRequired");
  const auto found = session.formations.find(formation_id);
  if (found == session.formations.end() ||
      found->second.formation_id != formation_id)
    return fail("FieldArmyExitActualFormationRequired");
  const auto &formation = found->second;
  if (formation.state == FieldFormationState::Withdrawn ||
      formation.state == FieldFormationState::Destroyed || formation.fit.empty())
    return fail("FieldArmyExitActiveFormationRequired");
  if (formation.side < 0 || formation.side > 1 ||
      selected_exit >= field.cells.size() ||
      std::find(session.sides[formation.side].exits.begin(),
                session.sides[formation.side].exits.end(), selected_exit) ==
          session.sides[formation.side].exits.end())
    return fail("FieldArmyExitOriginalExitRequired");
  std::map<StableId, FieldMotionPrefixFormation> current;
  for (const auto &[id, row] : session.formations) {
    if (id != row.formation_id ||
        row.fit.size() > std::size_t(std::numeric_limits<Integer>::max()))
      return fail("FieldArmyExitActualFormationRequired");
    current.emplace(id, FieldMotionPrefixFormation{
                            row.position_dm, Integer(row.fit.size()),
                            row.state == FieldFormationState::Routing,
                            row.state == FieldFormationState::Withdrawn ||
                                row.state == FieldFormationState::Destroyed,
                            id == formation_id ||
                                std::holds_alternative<FieldWithdraw>(row.mission)});
  }
  // 最终Gate腿复用原withdraw准入；这是已选出口的真实离场目的，不改他人任务。
  // 本入口先核一步时长；原函数继续核格网、兵种速度、路径、外接圆碰撞及到达点。
  const FieldMotionPrefixRead prefix{step.from, current};
  return read_field_navigation_step_at_prefix(
      session, field, formation_id, field_cell_center_dm(field, selected_exit),
      step, &prefix);
}
} // namespace hanmo::resident
