#include <string_view>
// 精确死亡来源读与原唯一DeathRef；身体改变仅由S4 condition实际写口执行。
#pragma once
#include "family_death.hpp"
#include "family_life_contract.hpp"
#include "resident/domains/military/military_exact_sources.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct FieldBattleWorldState;
class FieldSegmentApplicationRead;

struct LifeCauseView4 {
  StableId actor_id;
  SourceRef cause_ref;
  WorldMoment occurred_at;
  Integer source_schema = 0, source_ordinal = 0;
};
class NaturalAgeDeathSourceRead {
public:
  virtual ~NaturalAgeDeathSourceRead() = default;
  virtual Outcome<LifeCauseView4>
  read_age_death_source(const NaturalAgeDeathCause4 &) const = 0;
};
// 海事原NamedSeaOutcome提供；没有实际provider必须unsupported，不能填假battle。
class EnvironmentalSeaDeathSourceRead {
public:
  virtual ~EnvironmentalSeaDeathSourceRead() = default;
  virtual Outcome<LifeCauseView4>
  read_sea_death_source(const EnvironmentalSeaDeathCause4 &) const = 0;
};
struct FamilyLifeCauseSources4 {
  const MilitaryState &military;
  const MilitaryExactState *military_exact = nullptr;
  const NaturalAgeDeathSourceRead *age = nullptr;
  const EnvironmentalSeaDeathSourceRead *sea = nullptr;
  const MilitaryFormationSourceRead *military_formations = nullptr;
  const MilitaryReturnSourceRead *military_returns = nullptr;
  // 新 Complete 入口必须借实际不可变目录；旧严格 ref 构造不猜新选型。
  const CatalogIdentity *catalog_selection = nullptr;
  // Only the explicit source1/Condition8 + field_battle1 composition admits
  // this original root. It never replaces dynamic2 or fabricates a clock.
  const FieldBattleWorldState *field_battles = nullptr;
  const FieldSegmentApplicationRead *field_application_clock = nullptr;
  FamilyLifeCauseSources4(const MilitaryState &m, const MilitaryExactState &e,
                          const NaturalAgeDeathSourceRead *a = nullptr,
                          const EnvironmentalSeaDeathSourceRead *s = nullptr,
                          const MilitaryFormationSourceRead *f = nullptr,
                          const MilitaryReturnSourceRead *r = nullptr)
      : military(m), military_exact(&e), age(a), sea(s), military_formations(f),
        military_returns(r) {}
  FamilyLifeCauseSources4(
      const CatalogIdentity &catalog, const MilitaryState &m,
      const MilitaryExactState *e, const NaturalAgeDeathSourceRead *a = nullptr,
      const EnvironmentalSeaDeathSourceRead *s = nullptr,
      const MilitaryFormationSourceRead *f = nullptr,
      const MilitaryReturnSourceRead *r = nullptr,
      const FieldBattleWorldState *field = nullptr,
      const FieldSegmentApplicationRead *field_clock = nullptr)
      : military(m), military_exact(e), age(a), sea(s), military_formations(f),
        military_returns(r), catalog_selection(&catalog), field_battles(field),
        field_application_clock(field_clock) {}
};
// 同目录有限选择及实际根配对；Field1只由其唯一原段reader准入，不从未选推来源。
Status validate_family_life4_original_military_selection(
    const CommonStore &, const FamilyLifeCauseSources4 &);
Outcome<LifeCauseView4>
read_life_cause4(const CommonStore &, const FamilyLifeCauseSources4 &,
                 const LifeCauseRef4 &, const WorldMoment *actual_now = nullptr,
                 MilitaryCasualtyHistoryOrder ordering =
                     MilitaryCasualtyHistoryOrder::legacy_serial);
// Only an actual-prefix source owner opts in. Existing public callers retain
// legacy ordering; neither mode replaces the original cause/source validation.
Status validate_family_death4(const CommonStore &, const FamilyDeathRecord &,
                              const FamilyLifeCauseSources4 &,
                              const WorldMoment &current,
                              MilitaryCasualtyHistoryOrder ordering =
                                  MilitaryCasualtyHistoryOrder::legacy_serial);
Status validate_family_life4(const CommonStore &, const FamilyDeathState &,
                             const FamilyLifeCauseSources4 &,
                             const WorldMoment &current,
                             MilitaryCasualtyHistoryOrder ordering =
                                 MilitaryCasualtyHistoryOrder::legacy_serial);
// 完整原死亡验证及唯一actor反扫后的同World借行；不复制死亡/因果深值。
// Field1内部沿原fatal借证据不构造LifeCauseView4；dynamic/age/sea仍沿原provider接口，
// 其临时拥有边界不被本借行包装宣称已闭。
// 行只活到该候选根变更/回滚，调用者不得保存跨World或跨发布使用。
Outcome<const FamilyDeathRecord *>
read_verified_death4_record(const CommonStore &, const FamilyDeathState &,
                            const FamilyLifeCauseSources4 &, const DeathRef &,
                            const WorldMoment &current,
                            MilitaryCasualtyHistoryOrder ordering =
                                MilitaryCasualtyHistoryOrder::legacy_serial);
Outcome<VerifiedDeathView>
read_verified_death4(const CommonStore &, const FamilyDeathState &,
                     const FamilyLifeCauseSources4 &, const DeathRef &,
                     const WorldMoment &current,
                     MilitaryCasualtyHistoryOrder ordering =
                         MilitaryCasualtyHistoryOrder::legacy_serial);
class FamilyVerifiedDeathRead final : public VerifiedDeathRead {
public:
  FamilyVerifiedDeathRead(const CommonStore &, const FamilyDeathState &,
                          const FamilyLifeCauseSources4 &,
                          const ReadStampAuthority &,
                          const DomainReadVersionSource &,
                          MilitaryCasualtyHistoryOrder ordering =
                              MilitaryCasualtyHistoryOrder::legacy_serial);
  Outcome<VerifiedDeathView> by_ref(const DeathRef &,
                                    const ReadStamp &) const override;
  Outcome<std::optional<VerifiedDeathView>>
  by_actor(const StableId &, const ReadStamp &) const override;

private:
  const CommonStore &common_;
  const FamilyDeathState &state_;
  const FamilyLifeCauseSources4 &sources_;
  const ReadStampAuthority &stamps_;
  const DomainReadVersionSource &versions_;
  // 只有实际前缀来源owner显式传入；旧调用保持原legacy顺序。
  MilitaryCasualtyHistoryOrder ordering_;
};
Status visit_family_life4(CodecVisitor &, FamilyDeathState &);
Outcome<BoundaryValue> encode_family_life4(const FamilyDeathState &);
Outcome<FamilyDeathState> decode_family_life4(const BoundaryValue &);
Status emit_family_life4_projection(ProjectionWriter &, std::string_view,
                                    const FamilyDeathState &);
} // namespace hanmo::resident
