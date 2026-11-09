// 仅几何组件正反例；既有Field target调用，无独立main或另一正式平地世界。
#include "resident/domains/military/field_battle/field_army_exit_rendezvous.hpp"
#include "resident/domains/military/field_battle/field_army_exit_source.hpp"
#include "resident/domains/military/field_battle/field_routing.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
// 几何叶只读；失败必须指出原边界，不把任意早错当作覆盖。
void require_exit_step(bool value, const char *reason) {
  if (!value) throw std::runtime_error(reason);
}
}
// 原一步A*在Ended之后可读取，但不重写result、位置或旧Withdrawn来源。
void check_field_post_result_exit_geometry() {
  GeneratedBattleField field;
  field.field_hash = "geometry-component";
  field.columns = field.parameters.length_m / field.parameters.cell_m;
  field.rows = field.parameters.width_m / field.parameters.cell_m;
  field.cells.resize(std::size_t(field.columns) * field.rows);
  FieldBattleSession session;
  session.battle_id = "battle-component";
  session.field_hash = field.field_hash;
  session.state = FieldBattleState::Ended;
  session.now = {0, 100};
  session.result = FieldBattleResult{1, "FieldSideDefeated", session.now};
  const std::size_t first = std::size_t(field.columns) * 10 + 10;
  const std::size_t exit = first + 3;
  session.sides[0].exits = {exit};
  FieldFormation formation;
  formation.formation_id = "formation"; formation.army_id = "army";
  formation.fit = {"member"}; formation.ranks = 1;
  formation.state = FieldFormationState::Holding;
  formation.position_dm = field_cell_center_dm(field, first);
  session.formations.emplace(formation.formation_id, formation);
  const FieldBattleWindowSlice step{{0,100}, {0,110}, 0, 1, false, false};
  auto moved = read_field_post_result_exit_step(session, field, formation.formation_id, exit, step);
  require_exit_step(moved.ok && moved.value.to_dm.x > moved.value.from_dm.x &&
                    !moved.value.reached, "post-result original speed cannot teleport to exit");
  require_exit_step(session.formations.at(formation.formation_id).position_dm.x == formation.position_dm.x &&
                    session.result->ended.t == 100 && session.now.t == 100,
                    "read does not move formation, time or original battle terminal");
  auto invalid = read_field_post_result_exit_step(session, field, formation.formation_id, first, step);
  require_exit_step(!invalid.ok && invalid.error.original.code == "FieldArmyExitOriginalExitRequired",
                    "arbitrary gathering point is not an exit");
  // 不可达路径原读口提前返回blocked；入口仍必须先拒绝伪造步终点。
  auto blocked = field;
  for (auto &cell : blocked.cells) cell.terrain = FieldTerrainKind::Water;
  blocked.cells[first].terrain = FieldTerrainKind::Plain;
  blocked.cells[exit].terrain = FieldTerrainKind::Plain;
  auto wrong_time = step;
  wrong_time.to = {0, 120};
  invalid = read_field_post_result_exit_step(session, blocked, formation.formation_id, exit, wrong_time);
  require_exit_step(!invalid.ok && invalid.error.original.code == "FieldMotionActualStepRequired",
                    "unreachable exit cannot admit a forged step end");
  session.formations.at(formation.formation_id).state = FieldFormationState::Withdrawn;
  invalid = read_field_post_result_exit_step(session, field, formation.formation_id, exit, step);
  require_exit_step(!invalid.ok && invalid.error.original.code == "FieldArmyExitActiveFormationRequired",
                    "already exited formation cannot be resurrected into rally");
  // AI使用原幸存兵员加权而非主将点；无候选与重复候选均不能伪成选择。
  std::vector<FieldArmyExitWeight> weights{
      {"light", field_cell_center_dm(field, first), 1, 0},
      {"heavy", field_cell_center_dm(field, exit), 8, 2}};
  auto selected = read_field_army_weighted_exit_cell(field, weights, {first, exit});
  require_exit_step(selected.ok && selected.value == exit,
                    "AI exit weighs all surviving formation soldiers");
  selected = read_field_army_weighted_exit_cell(field, weights, {});
  require_exit_step(!selected.ok && selected.error.original.code == "FieldArmyExitReachableDestinationRequired",
                    "no eligible route cannot fabricate an exit");
  selected = read_field_army_weighted_exit_cell(field, weights, {first, first});
  require_exit_step(!selected.ok && selected.error.original.code == "FieldArmyExitOriginalExitRequired",
                    "duplicate eligibility is not another exit");
  // 真routing出口只逃散普通兵；兼任军籍的活主将、副将仍分别保fit/wounded。
  FieldFormation routed;
  routed.state = FieldFormationState::Routing;
  routed.commander_actor_id = "commander"; routed.deputy_actor_id = "deputy";
  routed.fit = {"ordinary", "commander"}; routed.wounded = {"deputy"};
  auto retained = settle_field_routing_deserters_at_exit(routed);
  require_exit_step(retained.ok && routed.fit == std::vector<StableId>{"commander"} &&
                    routed.wounded == std::vector<StableId>{"deputy"} &&
                    routed.deserted == std::vector<StableId>{"ordinary"},
                    "ordinary routing cannot desert a living army officer");
  session.state = FieldBattleState::Running;
  invalid = read_field_post_result_exit_step(session, field, formation.formation_id, exit, step);
  require_exit_step(!invalid.ok && invalid.error.original.code == "FieldArmyExitActualPostResultStepRequired",
                    "post-result geometry does not start before real terminal");
}
} // namespace hanmo::resident

#include "resident/domains/military/field_battle/field_army_exit_return.hpp"
#include "resident/domains/military/field_battle/field_ability_sources.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
namespace hanmo::resident {
// 既有门传入真实初始World借根；仅反例参数损坏，不伪造ready/晚采用bank或死亡事实。
void check_field_army_exit_ready_source_guards(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a) {
  const SourceRef absent{"military", "field_army_exit_ready", "missing-ready-negative"};
  const auto missing = read_field_army_exit_return_wait(x, catalog, a, absent);
  require_exit_step(!missing.ok && missing.error.original.code == "CommonActualFactHeaderRequired",
                   "missing ready cannot be classified as CommanderUnavailable waiting");
  const auto wrong = read_field_army_exit_return_wait(x, catalog, a,
      {"military", "campaign_arrival", absent.id});
  require_exit_step(!wrong.ok && wrong.error.original.code == "FieldArmyExitReadySourceRequired",
                   "ordinary campaign source cannot masquerade as Field ready");
  auto missing_period = x;
  missing_period.periods = nullptr;
  auto same_roots = a;
  same_roots.exact_contacts = &missing_period;
  const auto missing_owner = read_field_army_exit_return_wait(missing_period, catalog, same_roots, absent);
  require_exit_step(!missing_owner.ok && missing_owner.error.original.code == "FieldArmyExitReadySourceRequired",
                   "missing original period owner is not a business waiting result");
}
} // namespace hanmo::resident
