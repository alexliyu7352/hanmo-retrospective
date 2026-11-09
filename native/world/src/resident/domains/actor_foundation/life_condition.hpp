// 同一实际cause：唯一Condition身体写入、唯一FamilyDeathRef；整候选由WorldBundle提交。
#pragma once
#include "condition_capture.hpp"
#include "resident/domains/family/family_life_exact.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct LifeConditionApplication {
  DeathRef death_ref;
  ConditionReceiptRefV2 receipt_ref;
  std::vector<BoundActorConditionWrite> actual_writes;
  bool already_applied = false;
};
Outcome<LifeConditionApplication> apply_verified_life_termination(
    CommonStore &, AuthoredActorFoundationState &, FamilyDeathState &,
    const ActorFoundationCatalog &, const FamilyLifeCauseSources4 &,
    const LifeCauseRef4 &, const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &,
    const CharacterConditionSources *episode_sources = nullptr);
Status validate_life_termination_condition(
    const CommonStore &, const AuthoredActorFoundationState &,
    const FamilyDeathState &, const ActorFoundationCatalog &,
    const FamilyLifeCauseSources4 &, const ConditionReceiptV2 &,
    const WorldMoment &current,
    MilitaryCasualtyHistoryOrder ordering =
        MilitaryCasualtyHistoryOrder::legacy_serial);
Status validate_life_termination_coverage(
    const CommonStore &, const AuthoredActorFoundationState &,
    const FamilyDeathState &, const ActorFoundationCatalog &,
    const FamilyLifeCauseSources4 &, const WorldMoment &current,
    MilitaryCasualtyHistoryOrder ordering =
        MilitaryCasualtyHistoryOrder::legacy_serial);
} // namespace hanmo::resident
