// 原六个具名来源槽的真实拥有根身份；不建立事实、不增加释放权。
#pragma once
#include "character_availability_provenance.hpp"
#include <variant>
namespace hanmo::resident {
struct CharacterConditionSources;
class LifeAgeExactReadLease;
class CommonStore;
struct AuthoredActorFoundationState;
struct InitialConditionSeed;
struct CapacityTransitionV2;
struct MilitaryState;
struct MilitaryCasualtyFact;
struct NamedMilitaryCustodyTerm;
struct NamedMilitaryCustodyRelease;
struct FamilyDeathState;
struct FamilyDeathRecord;
struct LifeAgeState;
struct LifeAgeCatalog;
struct AgeCheckFact;
struct FactRecord;
template <class Owner, class Row> struct CharacterAvailabilitySourceOwnerRef {
  const Owner *owner = nullptr;
  const StableId *key = nullptr;
  const Row *row = nullptr;
  const FactRecord *header = nullptr;
};
using CharacterAvailabilitySourceOwnerIdentity = std::variant<
    CharacterAvailabilitySourceOwnerRef<AuthoredActorFoundationState,
                                        InitialConditionSeed>,
    CharacterAvailabilitySourceOwnerRef<CommonStore, FactRecord>,
    CharacterAvailabilitySourceOwnerRef<AuthoredActorFoundationState,
                                        CapacityTransitionV2>,
    CharacterAvailabilitySourceOwnerRef<MilitaryState, MilitaryCasualtyFact>,
    CharacterAvailabilitySourceOwnerRef<MilitaryState,
                                        NamedMilitaryCustodyTerm>,
    CharacterAvailabilitySourceOwnerRef<MilitaryState,
                                        NamedMilitaryCustodyRelease>,
    CharacterAvailabilitySourceOwnerRef<FamilyDeathState, FamilyDeathRecord>,
    CharacterAvailabilitySourceOwnerRef<LifeAgeState, AgeCheckFact>>;
enum class CharacterAvailabilitySourceReleaseCapability {
  individual_release_not_provided,
  original_condition_owner_gate,
  original_age_check_owner_gate
};
struct CharacterAvailabilitySourceRetentionRef {
  CharacterAvailabilityProvenanceSlot slot{};
  SourceRefBuildView
      source; // 只借本返回 identity 的真实 key；kind/domain 是原有限常量。
  Integer schema = 0;
  CharacterAvailabilitySourceOwnerIdentity identity;
  CharacterAvailabilitySourceReleaseCapability release =
      CharacterAvailabilitySourceReleaseCapability::
          individual_release_not_provided;
};
struct CharacterAvailabilitySourceRetentionContext {
  const CharacterConditionSources &condition;
  const LifeAgeExactReadLease *age = nullptr;
  // 中央 actual World owning 根，不是从 lease 值反推的另一份副本。
  // 已选 age 来源必须同时有这两个地址；wrapper provider 指针可与内adapter不同。
  const LifeAgeState *actual_age_state = nullptr;
  const LifeAgeCatalog *actual_age_catalog = nullptr;
};
// 仅同步不可变同 World
// 租约有效。根修改、rollback、clone/decode/替换即使引用失效。 source 必须是原
// provenance 六个 established 槽之一；未知源/版本拒，不能猜 schema。 返回
// release capability 不是 release 许可，中央仍须完整 PIN 与原 owner gate。
Outcome<CharacterAvailabilitySourceRetentionRef>
resolve_character_availability_source_retention_ref(
    const CharacterAvailabilitySourceRetentionContext &,
    CharacterAvailabilityProvenanceSlot, SourceRefBuildView,
    Integer actual_schema);
Status validate_character_availability_source_retention_ref(
    const CharacterAvailabilitySourceRetentionContext &,
    const CharacterAvailabilitySourceRetentionRef &);
} // namespace hanmo::resident
