// The original calendar year check and Life4 writer share one world candidate.
#pragma once
#include "life_age.hpp"
#include "resident/domains/actor_foundation/life_condition.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
class GrowthCapabilityRevisionWrite;
class CareLifeConditionWrite;
class CharacterConditionWorldRead;
struct CharacterConditionSources;
// scenario.life_age_checks is an explicit selection of the original rule.
// Only the current controlled actor's natural-death setting is configurable.
Outcome<LifeAgeCatalog> decode_life_age_catalog(const BoundaryValue &,
                                                const CatalogIdentity &,
                                                const OriginalCalendarRead &);
Status initialize_life_age_state_from_original_seed(const LifeAgeSources &,
                                                    const LifeAgeCatalog &,
                                                    LifeAgeState &,
                                                    const FamilyDeathState &,
                                                    const WorldMoment &);
struct LifeAgeExactReadBinding {
  const LifeAgeSources &sources;
  const LifeAgeCatalog &catalog;
  const LifeAgeState &state;
  const FamilyDeathState &deaths;
  const FamilyLifeCauseSources4 &life_sources;
  const CurrentMomentRead &clock;
};
class LifeAgeExactReadLease {
public:
  virtual ~LifeAgeExactReadLease() = default;
  virtual LifeAgeExactReadBinding bindings() const = 0;
};
// Concrete borrowed adapters. OriginalCalendarRead/control/clock must be owned
// earlier by the same complete-world lease; no current Moment is cached here.
class NativeLifeAgeExactRead final : public LifeAgeExactReadLease,
                                     public NaturalAgeDeathSourceRead {
  LifeAgeSources sources_;
  const LifeAgeCatalog &catalog_;
  const LifeAgeState &state_;
  const FamilyDeathState &deaths_;
  const CurrentMomentRead &clock_;
  FamilyLifeCauseSources4 life_sources_;

public:
  NativeLifeAgeExactRead(
      LifeAgeSources, const LifeAgeCatalog &, const LifeAgeState &,
      const FamilyDeathState &, const CurrentMomentRead &,
      const MilitaryState &, const MilitaryExactState &,
      const EnvironmentalSeaDeathSourceRead *,
      const MilitaryFormationSourceRead *formations = nullptr,
      const MilitaryReturnSourceRead *returns = nullptr);
  // 同实际目录选型的入口，不为未选旧军事构造空根。
  NativeLifeAgeExactRead(
      LifeAgeSources, const LifeAgeCatalog &, const LifeAgeState &,
      const FamilyDeathState &, const CurrentMomentRead &,
      const CatalogIdentity &, const MilitaryState &,
      const MilitaryExactState *, const EnvironmentalSeaDeathSourceRead *,
      const MilitaryFormationSourceRead *formations = nullptr,
      const MilitaryReturnSourceRead *returns = nullptr,
      const FieldBattleWorldState *field = nullptr,
      const FieldSegmentApplicationRead *field_clock = nullptr);
  LifeAgeExactReadBinding bindings() const override;
  Outcome<LifeCauseView4>
  read_age_death_source(const NaturalAgeDeathCause4 &) const override;
};
struct LifeAgeExactWriteBinding {
  const LifeAgeExactReadLease &original;
  CommonStore &common;
  AuthoredActorFoundationState &foundation;
  LifeAgeState &state;
  FamilyDeathState &deaths;
  const CurrentReadStampRead &current;
  const ReadStampAuthority &stamps;
  const DomainReadVersionSource &versions;
  GrowthCapabilityRevisionWrite *capability_changes = nullptr;
  CareLifeConditionWrite *care_life = nullptr;
  // 借同一完整世界 owner；只在写入前取 sources，不跨写边界保存借值。
  // 真寿终必须提供，空指针仅表达缺失并硬拒，不是无伤/空来源默认值。
  const CharacterConditionWorldRead *character_conditions = nullptr;
};
// 只核寿终 reader 与原 body 的候选/来源/时钟绑定；不替代唯一生命
// writer 的因果、Condition 与伤损 episode 完整校验，不创建来源或事实。
Status
validate_life_age_death_condition_sources(const LifeAgeExactReadBinding &,
                                          const CharacterConditionSources *,
                                          const WorldMoment &actual_at);
class LifeAgeExactWriteLease {
public:
  virtual ~LifeAgeExactWriteLease() = default;
  virtual LifeAgeExactWriteBinding bindings() = 0;
};
class NativeLifeAgeExactWrite final : public LifeAgeExactWriteLease {
  LifeAgeExactWriteBinding binding_;

public:
  explicit NativeLifeAgeExactWrite(LifeAgeExactWriteBinding binding)
      : binding_(binding) {}
  LifeAgeExactWriteBinding bindings() override { return binding_; }
};
struct LifeAgeExactWorldFactory {
  Outcome<std::shared_ptr<const LifeAgeExactReadLease>> (*read)(
      const WorldBundle &, const WorldMoment &) = nullptr;
  Outcome<std::shared_ptr<LifeAgeExactWriteLease>> (*write)(
      WorldBundle &, const WorldMoment &) = nullptr;
  // The central original live-capture consumer applies the new Condition input
  // before birth/guardianship. It must not publish or start another capture.
  Status (*after_actual_life_write)(WorldBundle &,
                                    ExactPhaseWriteContext &) = nullptr;
  // 已选控制根才必需；只在原 Death4 写出后同候选采用真实待接续，不切换控制。
  Status (*after_actual_death)(WorldBundle &, const DeathRef &) = nullptr;
};
Status apply_original_life_age_calendar_in_candidate(WorldBundle &,
                                                     const ExactPeriodContext &,
                                                     ExactPhaseWriteContext &,
                                                     LifeAgeExactWorldFactory);
// 原年龄阶段与明确后置空阶段的组合共用此正文，不另构一份阶段图。
Status validate_original_life_age_author_selection(const WorldBundle &,
                                                   LifeAgeExactWorldFactory);
Status validate_original_life_age_world_sources(const WorldBundle &,
                                                const WorldMoment &,
                                                LifeAgeExactWorldFactory);
// This registration covers its stated real age stage only. Other Family
// lifecycle stages must also be supplied before the complete profile is ready.
ExactPhaseRegistration
    make_life_age_exact_phase_registration(LifeAgeExactWorldFactory);
} // namespace hanmo::resident
