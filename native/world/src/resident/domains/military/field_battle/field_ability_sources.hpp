#pragma once
// §13.3 只读原人物、兵种和已学来源；图鉴或兵种自带列表不能冒习得。
#include "field_session.hpp"
#include "resident/domains/growth/growth_naval_history.hpp"
#include "resident/domains/military/military_tactic_learning_model.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryCatalog;
struct CommonCatalog;
class MilitaryFormationSourceRead;
class CurrentMomentRead;
class OriginalWorldReadBoundary;
class CheckpointMemoryCharge;
class FamilyLifeHistoryRead4;
struct FieldOriginalRosterReadContext;
struct CharacterConditionSources;
struct GrowthPermanentSources;
class CharacterAttributeAbsentSourcesReadV1;
// 同一 World 的原作者与习得根；phase 仅在真实接触安装候选内借用，会话不持有。
class FieldBattleActiveMembershipRead;
struct FieldExactContactReadContext;
struct PoliticalRelationsReadContext;
class PoliticalTakeoverTerritoryAccess;
struct FieldBattleAbilityReadContext {
  const CommonCatalog &common;
  const MilitaryCatalog &military;
  // 实际同 World 永久成长账和身份源；不以临时空根或今日技能代替冻结边界。
  const GrowthState &growth;
  GrowthNavalHistoryContext growth_history;
  const MilitaryTacticLearningState *learning = nullptr;
  const DynamicStepInput *phase = nullptr;
  // 同 job 保活的原成军来源与实际 S0 时钟；精确选择不能借 tick+0 代替。
  const MilitaryFormationSourceRead *original_formations = nullptr;
  const CurrentMomentRead *actual_clock = nullptr;
  const FamilyLifeHistoryRead4 *original_life = nullptr;
  const FieldOriginalRosterReadContext *original_rosters = nullptr;
  const CharacterConditionSources *conditions = nullptr;
  const GrowthPermanentSources *permanent_attributes = nullptr;
  const CharacterAttributeAbsentSourcesReadV1 *attribute_absence = nullptr;
  const FieldBattleActiveMembershipRead *active_membership = nullptr;
  const FieldExactContactReadContext *exact_contacts = nullptr;
  const PoliticalRelationsReadContext *diplomacy = nullptr;
  const PoliticalTakeoverTerritoryAccess *territory = nullptr;
  const OriginalWorldReadBoundary *original_read_boundary = nullptr;
};

Outcome<WorldMoment>
read_field_actual_application_moment(const CommonStore &,
                                     const FieldBattleAbilityReadContext &);

// 只读临时原来源投影；不是第二人物根，也不持有另一份 XP 账。
struct FieldOfficerStatistics {
  ActorAttributes attributes;
  Integer infantry = 0, cavalry = 0, archery = 0, tactics = 0;
};
class CommonStore;
struct FactRecord;
// 空 actual_before 只允许真实当前零时或原 contact_install phase；历史必须原
// Common 头。
Outcome<FieldOfficerStatistics>
read_field_officer_statistics(const CommonStore &,
                              const FieldBattleAbilityReadContext &,
                              const StableId &actor, Integer observed_tick,
                              const FactRecord *actual_before = nullptr);
// 只借实际同 World 的 Adopted 军议行：NPC 安装沿 Session/phase，
// 玩家零时定案沿军议采用头；不能把 Speak 头或今日技能倒填到旧定案。
struct FieldBattleWorldState;
Outcome<FieldOfficerStatistics>
read_field_officer_statistics_before_council_adoption(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    const FieldBattleWorldState &, const FieldCouncilFact &,
    const StableId &actor);

Outcome<FieldOfficerStatistics> read_field_officer_statistics_before_stratagem(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    const FieldBattleWorldState &, const FieldBattleSession &,
    const FieldStratagemRecord &, const StableId &actor);
// 原独立将领/已指定副手的当前真实指挥资格；局部阵亡立即生效，
// 不把 action 非空当缺席，不借尚未写回 Common 的存活标记复活将领。
Outcome<StableId> read_field_effective_commander(const CommonStore &,
                                                 const FieldFormation &);
struct MilitaryBattleCatalog;
struct FieldFormationAbilitySource {
  // Temporary actual source owns its strings/vectors through every consumer.
  CheckpointMemoryCharge copied_payload;
  StableId formation_id, commander_actor_id;
  std::vector<FieldTrait> traits;
  std::vector<FieldAvailableTactic> tactics;
  Integer tactic_capacity = 0;
};
// 输入来自同一已验 World；stable/真实 phase 边界交原已学读口验证。
// Lost 时读取已指定副手自身能力；side_commander
// 须为当前实际指挥者，不沿逝者能力。
// 这里只返回兵种与原已学的实际交集，不扣点；完整教学出处仍由原 World validator
// 核验。
Outcome<FieldFormationAbilitySource> read_field_formation_abilities(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    const MilitaryBattleCatalog &, const MilitaryTacticLearningCatalog &,
    const MilitaryTacticLearningState &, const FieldFormation &,
    const StableId &side_commander_actor_id, Integer observed_tick,
    const DynamicStepInput *phase = nullptr,
    const StableId &actual_before_fact = {},
    const WorldMoment *actual_consumer_at = nullptr);
} // namespace hanmo::resident

namespace hanmo::resident {
Status validate_field_battle_start_ability_sources(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    const FieldBattleSession &);
// 全部原能力来源读取成功后才改任何编队，失败不留下半 Running。
Status initialize_field_battle_abilities(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    FieldBattleSession &, const SourceRef &actual_start_source,
    Integer observed_tick, const std::optional<StableId> &started_by_actor_id,
    CheckpointMemoryCharge &);
// One actual queue admission only. The original entry header/event must exist
// in this candidate before the learned/Growth/effective prefix is captured.
Status initialize_field_reinforcement_ability_in_candidate(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    const FieldBattleWorldState &, FieldBattleSession &, FieldFormation &,
    CheckpointMemoryCharge &);
// 当前只验证真实开战边界，不冒脉冲后的快照；后续 driver 状态仍明确未实现。
struct FieldBattleWorldState;
// 借用原持久头；只定位首次实际参战的来源，不补点或建立新事实。
struct FieldAbilitySourceBoundary {
  const SourceRef *source = nullptr;
  Integer observed_tick = 0;
  FieldBattleTime joined_at;
  const WorldMoment *actual_at = nullptr;
};
Outcome<FieldAbilitySourceBoundary> read_field_ability_source_boundary(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldFormation &,
    const CurrentMomentRead *actual_clock = nullptr);
Status validate_field_battle_initial_abilities(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    const FieldBattleWorldState &, const FieldBattleSession &,
    const FieldBattleParameters &);
} // namespace hanmo::resident

namespace hanmo::resident {
class CheckpointMemoryCharge;
// 只读实际已冻结能力和本次本地时刻；权限由原命令/到令入口先验，不能白送习得。
Status validate_field_tactic_activation(const CommonStore &,
                                        const FieldFormation &,
                                        const FieldBattleTime &,
                                        const StableId &tactic_id,
                                        const FieldBattleParameters &);
// 同一候选发动一次；原点数、冷却、士气与效果同时写入，返回真实发动记录 ID。
// order_id 非空表示原传令已到；AI 来源由实际本场 driver 提供，不是公开参数。
Outcome<StableId> activate_field_tactic_in_candidate(
    CommonStore &, FieldBattleSession &, FieldFormation &,
    const StableId &issuer_actor_id, const StableId &tactic_id,
    const StableId *actual_order_id, bool ai_issued,
    const FieldBattleParameters &, CheckpointMemoryCharge &actual_world_owner);
// 只由真实步完成后调用；每 1800 战场秒补一点，不耗世界时间或 RNG。
Status recover_field_tactic_points_in_candidate(FieldBattleSession &,
                                                const FieldBattleParameters &);
} // namespace hanmo::resident

namespace hanmo::resident {
// The source is the actual military takeover application, not a death receipt
// and not the predecessor's initial skill snapshot.
Outcome<FieldFormationAbilitySource> read_field_commander_replacement_abilities(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    const FieldBattleWorldState &, const SourceRef &actual_change);
Status apply_field_commander_ability_replacement_in_candidate(
    const CommonStore &, const FieldBattleAbilityReadContext &,
    FieldBattleWorldState &, const SourceRef &actual_change,
    const FieldBattleParameters &, CheckpointMemoryCharge &);
} // namespace hanmo::resident
