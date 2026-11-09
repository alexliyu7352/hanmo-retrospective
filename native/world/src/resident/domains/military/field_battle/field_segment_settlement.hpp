// 连续野战每段的唯一原伤亡父源；只借同 World 状态，不建立第二份军人账。
#pragma once
#include "field_world.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
class CommonStore;
struct FieldBattleCatalog;
// 组合根只借本次实际 S0 scope；普通整数时钟或另一个 World 的 clock 不能代替。
class FieldSegmentApplicationRead : public CurrentMomentRead {
public:
  ~FieldSegmentApplicationRead() override = default;
  // Actual committed boundary or original S0 active closing span; a numeric
  // next tick is not sufficient. The complete World owner proves this scope.
  virtual Outcome<WorldMoment> read_field_window_closing_moment() const = 0;

  virtual bool bound_to(const CommonStore &, const MilitaryState &,
                        const FieldBattleWorldState &) const noexcept = 0;
};
Outcome<WorldMoment> read_field_segment_application_moment(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const SourceRef &, const FieldSegmentApplicationRead &);
Status validate_field_segment_application_sources(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const FieldSegmentApplicationRead &);

// 指针只在传入的同一个 World owner 生命周期内有效；终局事实在非终局段为空。
struct FieldSegmentSettlementSourceRead {
  const FieldBattleSession *session = nullptr;
  const FieldSegmentSettlement *settlement = nullptr;
  const MilitaryCasualtyFact *casualties = nullptr;
  const MilitaryBattleFact *terminal = nullptr;
};
// 验证真实 Common 父头、原段顺序与唯一回执；不把 Field 秒换算为全局精确时刻。
Outcome<FieldSegmentSettlementSourceRead> read_field_segment_settlement_source(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const SourceRef &,
    const FieldSegmentApplicationRead *actual_clock = nullptr);
// 真军死只借原段伤亡，不读取当前人物 alive 来抹去已发生的死亡。
// 该值不创建 DeathRef；人物唯一写者须在同候选消费，指针不跨 owner 生命周期。
struct FieldSegmentFatalSourceRead {
  FieldSegmentSettlementSourceRead source;
  const MilitaryActorChange *actor_change = nullptr;
  Integer source_ordinal = 0;
};
// actual_now 由同 World 原时钟提供；发生时刻只取原 settlement.applied_at，
// 不把战场秒换成全局时刻，也不把未提交的 formation.dead 当作死亡许可。
Outcome<FieldSegmentFatalSourceRead> read_field_segment_fatal_source(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const SourceRef &casualty, const StableId &actor_id,
    const WorldMoment &actual_now,
    const FieldSegmentApplicationRead *actual_clock = nullptr);
// Actual named captured outcome only. It does NOT prove the original body
// mutation or custody placement was applied and cannot release a group hold.
struct FieldSegmentCaptivitySourceRead {
  FieldSegmentSettlementSourceRead source;
  const MilitaryActorChange *actor_change = nullptr;
  const FieldBattleRoadBinding *captor_binding = nullptr;
  Integer source_ordinal = 0;
};
Outcome<FieldSegmentCaptivitySourceRead> read_field_segment_captivity_source(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const SourceRef &casualty, const StableId &actor_id,
    const FieldSegmentApplicationRead &actual_clock);
// 双向覆盖所有段头、伤亡父引用和终局事实；空段列表不证明 driver 已实现。
Status validate_field_segment_settlement_sources(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const FieldSegmentApplicationRead *actual_clock = nullptr);
// 仅在原 driver 已到窗末或终局后调用；同段重复不重发人物变化。
// 此函数只能写未发布的整 World 候选，后续人物/人口落账失败须整体丢弃。
Outcome<MilitaryBattleStep> settle_field_battle_segment_in_candidate(
    CommonStore &, MilitaryState &, FieldBattleWorldState &,
    const FieldBattleCatalog &, const StableId &battle_id,
    const FieldSegmentApplicationRead &actual_clock);
} // namespace hanmo::resident
