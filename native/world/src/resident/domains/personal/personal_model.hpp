// 个人战领域唯一类型化会话与作者定义；世界人物、资产和随机流仍由共同存储拥有。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <functional>
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
struct TaskCargoPolicy;
struct PersonalBattleLifecycleWriteContext;
class PersonalTerminalConditionWrite;
class PersonalInitialAttributesRead;
struct PersonalPoint {
  Integer x = 0, y = 0;
  bool operator==(const PersonalPoint &p) const { return x == p.x && y == p.y; }
  bool operator!=(const PersonalPoint &p) const { return !(*this == p); }
};
struct PersonalObstacle {
  Integer x = 0, y = 0, width = 0, height = 0;
  bool blocks_movement = false, blocks_sight = false;
  FieldOrder field_order;
};
struct PersonalSpawn {
  Integer side = 0, heading = 0;
  PersonalPoint position;
  FieldOrder field_order;
};
struct PersonalExit {
  Integer side = 0, radius = 0;
  PersonalPoint position;
  FieldOrder field_order;
};
struct PersonalArena {
  StableId id, name_key, visual_tag;
  Integer width = 0, height = 0;
  std::vector<PersonalObstacle> obstacles;
  std::vector<PersonalSpawn> spawns;
  std::vector<PersonalExit> exits;
  FieldOrder field_order;
};
struct PersonalWeapon {
  StableId category;
  Integer minimum_range = 0, maximum_range = 0, arc_degrees = 0, damage = 0,
          stamina_cost = 0, recovery_rounds = 0, move_distance = 0;
  FieldOrder field_order;
};
struct PersonalArea {
  StableId shape;
  Integer max_targets = 0;
  std::optional<Integer> half_width, arc_degrees;
  FieldOrder field_order;
};
struct PersonalSkillRequirement {
  StableId skill_id;
  Integer level = 0;
  FieldOrder field_order;
};
struct PersonalSkill {
  StableId id, name_key, description_key, kind, status;
  std::vector<StableId> weapons;
  Integer practice_ticks = 0, stamina_cost = 0, power_bp = 10000,
          defense_bp = 10000, move_bonus = 0, status_rounds = 0;
  std::optional<PersonalSkillRequirement> required_skill_level;
  std::optional<bool> requires_stationary;
  std::optional<Integer> maximum_range_bonus, hit_bonus_bp,
      defend_recovery_bonus, counter_damage_bp;
  std::optional<StableId> guard_facing;
  std::optional<PersonalArea> area;
  FieldOrder field_order, range_order, counter_order;
};
struct PersonalFacing {
  Integer side_bp = 11000, back_bp = 12500;
  FieldOrder field_order;
};
struct PersonalObjective {
  StableId id, name_key, kind;
  Integer max_rounds = 0, injury_on_defeat = 0;
  bool allow_withdrawal = false;
  std::optional<StableId> ai_model, escort_model, stamina_model, hit_model;
  std::optional<Integer> initial_stamina;
  std::optional<PersonalFacing> facing_damage;
  FieldOrder field_order;
};
struct PersonalTerms {
  StableId battlefield_id, objective_id;
  Integer fee = 0;
  StableId wallet_id;
  FieldOrder field_order;
  // 仅作者路遇目录可授权货物处置；开战冻结清单与回执归任务域保存。
  std::shared_ptr<const TaskCargoPolicy> cargo_policy;
};
struct PersonalCatalog {
  bool enabled = false;
  OrderedTable<PersonalWeapon> weapons;
  OrderedTable<Integer> armor_defenses;
  OrderedTable<PersonalSkill> skills;
  OrderedTable<PersonalObjective> objectives;
  OrderedTable<PersonalArena> arenas;
  OrderedTable<PersonalTerms> sparring, road_conflicts;
  // 显式情景只选择真实目标；属于不可变目录，不进入Personal1状态访客。
  std::vector<StableId> simultaneous_outcome_objectives;
};
struct PersonalSource {
  StableId kind, encounter_id;
  std::optional<InteractionContext> context;
  FieldOrder field_order;
};
struct PersonalPlan {
  PersonalPoint destination;
  Integer heading = 0;
  StableId action, target_id, skill_id;
  FieldOrder field_order;
};
struct PersonalHitLevels {
  Integer martial = 0, archery = 0;
  FieldOrder field_order;
};
struct PersonalUnit {
  StableId actor_id;
  Integer side = 0, heading = 0, radius = 18, hp = 0, initial_hp = 0,
          stamina = 50, martial = 1, recovery_until = 0;
  PersonalPoint position;
  StableId weapon_id, heart_id;
  OrderedTable<Integer> learned, statuses;
  bool protected_actor = false;
  std::optional<PersonalHitLevels> skill_levels;
  std::optional<StableId> armor_id;
  std::optional<Integer> armor_defense, withdrawn_round;
  FieldOrder field_order;
};
// 事件只保原规则明确字段；可选字段区分零值、空串和缺省，不持任意Variant。
struct PersonalEvent {
  std::optional<StableId> actor_id, target_id, action, skill_id, code, status;
  std::optional<Integer> damage, expires_round, amount, hit_bp, roll;
  std::optional<PersonalPoint> position;
  FieldOrder field_order;
};
// Selected personal_battle_lifecycle/1 adjunct. The original personal/1
// codec remains unchanged; this is the sole canonical per-battle value.
struct PersonalBattleLifecycleEvent1 {
  StableId source_id;
  WorldMoment occurred_at;
};
struct PersonalBattleLifecycle1 {
  PersonalBattleLifecycleEvent1 started;
  std::optional<WorldMoment> result_applied_at;
  std::optional<PersonalBattleLifecycleEvent1> closed;
};
struct PersonalBattle {
  StableId id, controller_id, battlefield_id, objective_id;
  PersonalSource source;
  Integer created_tick = 0, completed_tick = -1, round = 1;
  StableId phase = "planning", outcome, result_id;
  OrderedTable<PersonalUnit> units;
  OrderedTable<PersonalPlan> ai_plans;
  std::vector<PersonalEvent> last_events;
  bool applied = false;
  FieldOrder field_order;
  std::optional<PersonalBattleLifecycle1> lifecycle;
};
struct PersonalFact {
  StableId battle_id, result_id, outcome;
  Integer tick = 0;
  std::vector<StableId> participants, defeated;
  std::optional<std::vector<StableId>> withdrawn;
  PersonalSource source;
  FieldOrder field_order;
};
struct PersonalState {
  OrderedTable<PersonalBattle> battles;
  OrderedTable<PersonalFact> facts;
};
struct PersonalCommand {
  StableId kind, battle_id, encounter_id;
  std::optional<InteractionContext> context;
  std::optional<PersonalPlan> plan;
  std::optional<StableId> input_error;
};
struct PersonalPlanProjection;
struct PersonalResultProjection;
struct PersonalQuote {
  StableId kind, battle_id, controller_id, protected_id, battlefield_id,
      objective_id, wallet_id;
  Integer duration = 0, fee = 0;
  PersonalSource source;
  OrderedTable<Integer> sides;
  std::optional<PersonalPlan> plan;
  std::shared_ptr<const PersonalPlanProjection> tactical;
  std::shared_ptr<const PersonalResultProjection> result;
};
struct PersonalReceipt {
  StableId battle_id;
  std::optional<Integer> round;
  std::optional<StableId> outcome;
};
struct PersonalAccess {
  std::function<bool()> pauses_world;
  std::function<bool(const StableId &)> occupied;
  std::function<std::optional<std::pair<StableId, StableId>>(
      const StableId &, const OrderedTable<Integer> &)>
      escort_objective;
  std::function<Status(const PersonalBattle &)> record_escort;
  // 真实路遇会话插入及结果事实应用后调用；失败由中央丢弃整个候选。
  std::function<Status(const PersonalBattle &)> on_battle_started,
      on_battle_result;
  Integer selected_battle_lifecycle = 0;
  const PersonalBattleLifecycleWriteContext *battle_lifecycle = nullptr;
  // Explicit PreparedCatalog selection; a non-null writer never selects it.
  Integer selected_injury_sources = 0;
  PersonalTerminalConditionWrite *terminal_condition = nullptr;
  // Only source1 uses the same-World original effective-attribute reader.
  const PersonalInitialAttributesRead *initial_attributes = nullptr;
};
struct PersonalAttackTarget {
  StableId target_id;
  Integer damage = 0;
  std::optional<Integer> hit_bp;
};
struct PersonalAttackPreview {
  Integer damage = 0;
  std::optional<Integer> hit_bp;
  std::optional<std::vector<PersonalAttackTarget>> targets;
};
// 报价保存有限有类型公开投影，不借用可变战场，也不保存JSON或完整目录。
struct PersonalGoalProjection {
  StableId kind, goal_rule, timeout_rule;
  std::optional<StableId> mutual_defeat_rule;
  Integer max_rounds = 0, initial_stamina = 50;
  bool allow_withdrawal = false;
  std::vector<StableId> protected_actor_ids;
};
struct PersonalResultPerson {
  StableId actor_id, status;
  Integer side = 0, hp = 0, initial_hp = 0;
};
struct PersonalPlanProjection {
  bool ok = false;
  StableId code;
  Integer movement_distance = 0, nominal_stamina_cost = 0;
  std::vector<StableId> selectable, present, attackable;
  PersonalGoalProjection goal;
  std::vector<PersonalEvent> events;
  std::optional<PersonalWeapon> weapon;
  std::optional<bool> requires_stationary;
  std::optional<StableId> guard_facing;
  std::optional<Outcome<PersonalAttackPreview>> attack;
};
struct PersonalResultProjection {
  bool ok = false;
  StableId code, outcome;
  PersonalGoalProjection goal;
  std::vector<PersonalResultPerson> people;
  std::vector<StableId> defeated, withdrawn, reasons;
  std::vector<PersonalEvent> events;
};
PersonalPlanProjection personal_plan_projection(const PersonalBattle &,
                                                const PersonalCatalog &,
                                                const StableId &,
                                                const PersonalPlan &);
PersonalResultProjection personal_result_projection(const PersonalBattle &,
                                                    const PersonalCatalog &);
BoundaryValue encode_personal_plan_projection(const PersonalPlanProjection &);
BoundaryValue
encode_personal_result_projection(const PersonalResultProjection &);
// 冷边界和状态验证；所有调用失败由中央候选事务丢弃，绝不在热状态缓存原字典。
Outcome<PersonalCatalog> decode_personal_catalog(const BoundaryValue &);
Outcome<PersonalState> decode_personal_seed(const BoundaryValue &);
BoundaryValue encode_personal_state(const PersonalState &);
Status visit_personal_checkpoint(CodecVisitor &, PersonalState &);
Outcome<PersonalCommand> decode_personal_command(const StableId &,
                                                 const BoundaryValue &);
BoundaryValue encode_personal_command(const PersonalCommand &);
BoundaryValue encode_personal_quote(const PersonalQuote &);
BoundaryValue encode_personal_receipt(const PersonalReceipt &);
Outcome<PersonalReceipt> decode_personal_receipt(const StableId &,
                                                 const BoundaryValue &);
Status visit_personal_receipt(CodecVisitor &, PersonalReceipt &);
Status validate_personal(const CommonStore &, const PersonalState &,
                         const PersonalCatalog &, const InteractionState &);
Outcome<PersonalQuote>
preview_personal(const CommonStore &, const PersonalState &,
                 const PersonalCatalog &, const InteractionState &,
                 const InteractionCatalog &, const InteractionAccess &,
                 const PersonalAccess &, const StableId &,
                 const PersonalCommand &);
Outcome<PersonalReceipt>
apply_personal(CommonStore &, PersonalState &, const PersonalCatalog &,
               InteractionState &, const InteractionCatalog &,
               const InteractionAccess &, const PersonalAccess &,
               const StableId &, const PersonalQuote &);
bool personal_occupied(const PersonalState &, const StableId &);
bool personal_pauses_world(const PersonalState &);
std::vector<StableId> personal_held_travel_actors(const CommonStore &,
                                                  const PersonalState &,
                                                  const PersonalCatalog &);
Integer personal_skill_level(Integer);
Status personal_skill_eligible(const ActorRecord &, const PersonalSkill &);
OrderedTable<Integer> personal_learned_snapshot(const ActorRecord &,
                                                const PersonalCatalog &);
Outcome<PersonalPlan> personal_validate_plan(const PersonalBattle &,
                                             const PersonalCatalog &,
                                             const StableId &,
                                             const PersonalPlan &);
Outcome<PersonalAttackPreview> personal_preview_attack(const PersonalBattle &,
                                                       const PersonalCatalog &,
                                                       const StableId &,
                                                       const PersonalPlan &);
Outcome<OrderedTable<PersonalPlan>> personal_make_ai(const PersonalBattle &,
                                                     const PersonalCatalog &);
Outcome<PersonalBattle> personal_resolve(const PersonalBattle &,
                                         const PersonalCatalog &,
                                         const PersonalPlan &, CommonStore &);
Integer personal_movement_distance(const PersonalBattle &,
                                   const PersonalCatalog &, const StableId &,
                                   const StableId &skill_id = {},
                                   const StableId &action = {});
StableId personal_outcome(const PersonalBattle &, const PersonalCatalog &);
StableId personal_limit_outcome(const PersonalBattle &,
                                const PersonalCatalog &);
// 页面输出仅投影公开快照，未来AI意图和随机roll不进入玩家投影。
BoundaryValue personal_goal_view(const PersonalBattle &,
                                 const PersonalCatalog &);
BoundaryValue personal_events_view(const PersonalBattle &);
BoundaryValue personal_result_view(const PersonalBattle &,
                                   const PersonalCatalog &);
BoundaryValue personal_plan_view(const PersonalBattle &,
                                 const PersonalCatalog &, const StableId &,
                                 const PersonalPlan &);
BoundaryValue personal_battle_view(const CommonStore &, const PersonalState &,
                                   const PersonalCatalog &,
                                   const CommonCatalog &);
BoundaryValue personal_battle_context(const CommonStore &,
                                      const PersonalState &, const StableId &);
BoundaryValue personal_battle_geometry(const CommonStore &,
                                       const PersonalState &,
                                       const PersonalCatalog &,
                                       const StableId &, const PersonalPlan &);
} // namespace hanmo::resident
// 会话占用、开战半日与一次性世界结算由中央明确协调。
