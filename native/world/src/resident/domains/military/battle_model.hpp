// 既有六角军团会战的有限typed模型；不保存Godot对象、作者字典或另一份世界资产。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "hanmo/resident/model_contract.hpp"
#include "hanmo/resident/world_moment.hpp"
#include "military_combat_conditions_model.hpp"
#include "military_command_resources_model.hpp"
#include "military_command_shock_contract.hpp"
#include "military_commanded_damage_model.hpp"
#include "military_field_terrain_contract.hpp"
#include "military_rout_contract.hpp"
#include "military_sequential_contract.hpp"
#include "military_tactic_learning_model.hpp"
#include <functional>
#include <map>
namespace hanmo::resident {
// 轴向坐标沿原整数范围，键的字典序仅用于原Dijkstra稳定仲裁。
struct HexPoint {
  Integer q = 0, r = 0;
  bool operator==(const HexPoint &p) const { return q == p.q && r == p.r; }
  bool operator!=(const HexPoint &p) const { return !(*this == p); }
};
struct HexCell {
  Integer q = 0, r = 0;
  StableId terrain;
  std::optional<bool> blocks_movement, blocks_sight;
  std::optional<Integer> height, cover;
  FieldOrder field_order;
};
struct HexEntry {
  Integer side = 0;
  HexPoint position;
  FieldOrder field_order;
};
struct MilitaryDamageModel {
  StableId kind = "legacy_v1";
  Integer side_bp = 10000, rear_bp = 10000, forest_ranged_bp = 10000,
          hill_melee_bp = 10000, min_total_bp = 4000, max_total_bp = 18000;
  bool present = false, empty = true;
  FieldOrder field_order;
};
// 仅显式作者战场启用；人物伤亡仍入原 MilitaryCasualtyFact，不扩旧存档访客。
struct MilitaryCommanderLossRule {
  StableId kind = "field_exposure_v1";
  Integer exposure_bp = 0;
};
struct BattlefieldDefinition {
  StableId id, name, objective_id;
  std::optional<StableId> visual_tag;
  std::vector<HexCell> cells;
  std::vector<HexEntry> spawns, exits;
  MilitaryDamageModel damage_model;
  std::optional<MilitaryCommanderLossRule> commander_losses;
  std::optional<MilitaryRoutRetreatRule> rout_retreat;
  std::optional<MilitaryFieldTerrainRule> field_terrain;
};
struct MilitaryTactic {
  StableId id, kind, name;
  Integer point_cost = 0, morale_effect = 0, move_bonus = 0, status_rounds = 1,
          power_bp = 10000, accuracy_bonus = 0;
  StableId learned_tactic_id;
};
struct TroopDefinition {
  StableId id, name, category, portrait_kind;
  bool has_name_key = false;
  Integer movement = 0, minimum_range = 1, maximum_range = 1,
          tactic_capacity = 0, attack = 1, defense = 0, accuracy = 0;
  CheckpointMap<StableId, Integer> movement_costs;
  std::vector<MilitaryTactic> tactics;
};
struct BattleObjectiveDefinition {
  StableId id, name, kind;
  Integer side = 0, max_rounds = 1, hold_rounds = 0, required_units = 0;
  bool allow_withdrawal = false;
  std::optional<HexPoint> point;
};
struct MilitaryOrder {
  StableId action;
  HexPoint destination;
  Integer heading = 0;
  StableId target_id, tactic_id;
  FieldOrder field_order;
};
using MilitaryOrders = std::vector<std::pair<StableId, MilitaryOrder>>;
struct BattleUnit {
  StableId unit_id, army_id, commander_actor_id, troop_type_id;
  Integer side = 0;
  HexPoint position;
  Integer heading = 0, morale = 0, tactic_points = 0;
  std::vector<std::pair<StableId, Integer>> statuses;
  bool withdrawn = false;
  std::vector<StableId> member_ids, fit, wounded, dead, captured, deserted;
  FieldOrder field_order;
};
struct MilitaryObjectiveProgress {
  Integer hold_rounds = 0;
  std::vector<StableId> escaped_unit_ids;
  FieldOrder field_order;
};
struct MilitaryBattleResult {
  Integer winner_side = -1;
  StableId reason;
  FieldOrder field_order;
};
// 所有事件字段都有明确存在位；不能把未出现的数值字段补成零改变旧回执。
struct MilitaryBattleEvent {
  StableId kind, unit_id;
  std::optional<StableId> target_id, attacker_id, tactic_id, code,
      captor_unit_id, captor_army_id, reason;
  std::optional<Integer> morale, casualties, hit_chance, original_casualties,
      captured, deserted, winner_side;
  std::optional<HexPoint> position;
  std::optional<std::vector<StableId>> captured_ids;
  FieldOrder field_order;
};
// Explicit military_battle_lifecycle/1 adjunct; the original battle remains
// the sole owner. No result/occupation projection or second lifecycle table.
struct MilitaryBattleLifecycleEvent1 {
  StableId source_id;
  WorldMoment occurred_at;
};
struct MilitaryBattleLifecycle1 {
  MilitaryBattleLifecycleEvent1 started;
  std::optional<MilitaryBattleLifecycleEvent1> closed;
};
struct MilitaryBattleRecord {
  StableId id, kind = "military", battlefield_id, objective_id;
  std::vector<StableId> objective_unit_ids;
  StableId phase = "planning";
  Integer round = 1;
  std::vector<BattleUnit> units;
  MilitaryOrders ai_plans;
  MilitaryObjectiveProgress objective_state;
  StableId rng_seed = "0", rng_state = "0";
  FieldOrder rng_field_order;
  std::vector<MilitaryBattleEvent> last_events;
  std::optional<MilitaryBattleResult> result;
  Integer created_tick = 0, last_tick = 0, completed_tick = -1;
  StableId source_id;
  std::vector<StableId> army_ids, participant_actor_ids;
  bool plan_submitted = false;
  MilitaryOrders pending_orders;
  std::vector<StableId> round_receipt_ids;
  StableId settlement_receipt_id;
  bool applied = false;
  StableId controller_actor_id;
  Integer controller_side = 0;
  StableId command_scope = "unit", command_army_id;
  std::vector<StableId> command_unit_ids;
  StableId authority_office_id;
  FieldOrder field_order;
  // 仅Military10会话写入；由实际野外contact冻结，同board围城仍沿旧路径。
  bool rout_retreat_enabled = false;
  // 只在Military11写存在位；M10原永久帧会携带同一真实一次源。
  std::optional<MilitaryCommandShockState> command_shock;
  // 仅Military13写入；冻结真实入战前条件，回合中仍由原军事状态主权持有。
  std::optional<MilitaryCombatConditionState> combat_conditions;
  // 仅Military14冻结原真实入战人物资格；旧格式无存在位。
  std::optional<MilitaryCommandedDamageState> commanded_damage;
  // Military15单独存在位；仅冻结原兵法资格，点数仍由BattleUnit持有。
  std::optional<MilitaryCommandResourcesState> command_resources;
  // 仅Military16记录实际入场前本领；后学不会倒改在途会战。
  std::optional<MilitaryTacticEntryState> learned_tactics;
  // Not part of any original Military physical version; sole selected adjunct.
  std::optional<MilitaryBattleLifecycle1> lifecycle;
};
// supply_attrition只由原Military9目录解码派生，旧作者/存档不新增隐式开关。
struct MilitaryBattleCatalog {
  std::vector<BattlefieldDefinition> battlefields;
  std::vector<TroopDefinition> troop_types;
  std::vector<BattleObjectiveDefinition> objectives;
  bool supply_attrition = false;
  bool rout_retreat_enabled = false;
  bool field_terrain_enabled = false;
  std::optional<MilitaryCommandShockRule> command_shock_rule;
  std::optional<MilitaryCombatConditionRule> combat_condition_rule;
  std::optional<MilitaryCommandedDamageRule> commanded_damage_rule;
  std::optional<MilitarySequentialRoundRule> sequential_round_rule;
  std::optional<MilitaryCommandResourcesRule> command_resources_rule;
  std::optional<MilitaryTacticLearningCatalog> tactic_learning;
};
struct HexPath {
  HexPoint position;
  std::vector<HexPoint> path;
  Integer cost = 0;
};
struct MilitaryOrderQuote {
  MilitaryOrder order;
  std::vector<HexPoint> path;
  Integer cost = 0;
  std::optional<Integer> incoming_damage_bp, support_share_bp, hit_chance,
      casualties_min, casualties_max;
};
struct MilitaryAttackQuote {
  Integer hit_chance = 0, potential = 0, flank = 0;
};
using MilitaryDamageSources = std::vector<std::pair<StableId, Integer>>;
// 会话随机状态由同一个PCG服务逐次推进；失败必须由外层丢弃候选，不重抽本轮。
using MilitaryBattleRandom =
    std::function<Outcome<Integer>(StableId &, StableId &, Integer, Integer)>;
namespace military_hex {
StableId key(HexPoint);
Integer distance(HexPoint, HexPoint);
Integer direction(HexPoint, HexPoint);
std::vector<HexPoint> neighbors(HexPoint);
const HexCell *cell(const BattlefieldDefinition &, HexPoint);
Integer height(const BattlefieldDefinition &, HexPoint);
Integer cover(const BattlefieldDefinition &, HexPoint);
Integer movement_cost(const BattlefieldDefinition &, HexPoint,
                      const TroopDefinition &);
// 通行成本是几何输入；船型可直接借原路径引擎，无需伪造军旅兵种。
Integer movement_cost(const BattlefieldDefinition &, HexPoint,
                      const std::map<StableId, Integer> &);
bool line_of_sight(const BattlefieldDefinition &, HexPoint, HexPoint);
Status validate(const BattlefieldDefinition &);
Outcome<std::vector<HexPath>> reachable(const BattlefieldDefinition &, HexPoint,
                                        const std::map<StableId, Integer> &,
                                        Integer,
                                        const std::vector<HexPoint> & = {},
                                        const std::vector<HexPoint> & = {});
Outcome<std::vector<HexPath>> reachable(const BattlefieldDefinition &, HexPoint,
                                        const TroopDefinition &, Integer,
                                        const std::vector<HexPoint> & = {},
                                        const std::vector<HexPoint> & = {});
} // namespace military_hex
// 纯轮次入口：查验与预览不抽随机、不改真实名册或时钟，resolve只返回完整新会话。
Status validate_military_battle(const MilitaryBattleRecord &,
                                const MilitaryBattleCatalog &,
                                bool require_ai = false);
// 接舷复用原近战命中/人员伤害公式；只读实际单位，不调用陆上溃退或改写军种通行能力。
Outcome<MilitaryAttackQuote>
preview_military_close_attack(const BattleUnit &, const BattleUnit &,
                              const MilitaryBattleCatalog &,
                              const StableId &battlefield_id, Integer round);
// 舰載规则须先核本船作者武器与实际技艺来源；这里只给原人员战果的纯报价，不授行动权。
Outcome<MilitaryAttackQuote>
preview_military_afloat_attack(const BattleUnit &, const BattleUnit &,
                               const MilitaryBattleCatalog &,
                               const StableId &battlefield_id, Integer round,
                               Integer minimum_range, Integer maximum_range);
Outcome<MilitaryOrderQuote>
preview_military_order(const MilitaryBattleRecord &,
                       const MilitaryBattleCatalog &, const StableId &,
                       const MilitaryOrder &,
                       const MilitaryDamageSources & = {});
Outcome<std::vector<HexPath>>
military_movement_options(const MilitaryBattleRecord &,
                          const MilitaryBattleCatalog &, const StableId &,
                          const StableId &tactic_id = {});
Outcome<MilitaryOrders> validate_military_orders(const MilitaryBattleRecord &,
                                                 const MilitaryBattleCatalog &,
                                                 const StableId &,
                                                 const MilitaryOrders &);
Outcome<MilitaryOrders> make_military_ai(const MilitaryBattleRecord &,
                                         const MilitaryBattleCatalog &,
                                         const MilitaryDamageSources & = {});
Outcome<MilitaryBattleRecord>
resolve_military_round(const MilitaryBattleRecord &,
                       const MilitaryBattleCatalog &, const StableId &,
                       const MilitaryOrders &, const MilitaryBattleRandom &,
                       const MilitaryDamageSources & = {});
} // namespace hanmo::resident
