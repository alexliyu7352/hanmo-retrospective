// 原 availability 五类无 SourceRef 借源的有限拥有根身份；不是事实或持久账。
#pragma once
#include "resident/kernel/source_ref.hpp"
#include <variant>
namespace hanmo::resident {
class CommonStore;
struct CharacterConditionSources;
struct CharacterOriginRecordsStateV1;
struct CharacterOriginRecordV2;
struct RegistrationSourceV2;
struct InitialCapabilityAllocation;
struct BirthFactRef;
struct FactRecord;
struct FamilyBirthState;
struct FamilyBirthRecord;
// 已发布的原身份/出生根没有个别释放业务 API。原初始化失败可撤回整候选；
// clone/decode/整 World 替换也不是单行 release。此约束不等于所有 Common
// 后缀已被保全，原 typed source 和 Common prefix 依赖仍须独立完整登记。
enum class CharacterAvailabilityRetentionLifetime {
  published_root_owned_no_individual_release
};
struct CharacterOriginOwnedRetentionRef {
  const CharacterOriginRecordsStateV1 *owner = nullptr;
  const StableId *key = nullptr;
  const CharacterOriginRecordV2 *row = nullptr;
};
struct CharacterRegistrationOwnedRetentionRef {
  const CharacterOriginRecordsStateV1 *owner = nullptr;
  const StableId *key = nullptr;
  const RegistrationSourceV2 *row = nullptr;
};
struct CharacterBirthAllocationOwnedRetentionRef {
  const CharacterOriginRecordsStateV1 *owner = nullptr;
  const StableId *key = nullptr;
  const InitialCapabilityAllocation *row = nullptr;
};
struct CharacterOriginalBirthOwnedRetentionRef {
  const FamilyBirthState *owner = nullptr;
  const StableId *key = nullptr;
  const FamilyBirthRecord *row = nullptr;
  const FactRecord *header = nullptr;
  // 原 Origin2 内实际 BornCharacterSource.birth_record_ref 的借指针。
  const BirthFactRef *original_reference = nullptr;
};
struct CharacterBornCreationOwnedRetentionRef {
  const CommonStore *owner = nullptr;
  const StableId *key = nullptr;
  const FactRecord *header = nullptr;
  const FamilyBirthState *birth_owner = nullptr;
  const StableId *birth_key = nullptr;
  const FamilyBirthRecord *birth = nullptr;
};
using CharacterAvailabilityOwnedRetentionIdentity =
    std::variant<CharacterOriginOwnedRetentionRef,
                 CharacterRegistrationOwnedRetentionRef,
                 CharacterBirthAllocationOwnedRetentionRef,
                 CharacterOriginalBirthOwnedRetentionRef,
                 CharacterBornCreationOwnedRetentionRef>;
struct CharacterAvailabilityRetentionRef {
  CharacterAvailabilityOwnedRetentionIdentity identity;
  CharacterAvailabilityRetentionLifetime lifetime =
      CharacterAvailabilityRetentionLifetime::
          published_root_owned_no_individual_release;
};
struct CharacterAvailabilityRetentionContext {
  const CharacterConditionSources &condition;
  // 只选原 Birth4 时借同 World 根；不以非空根代替原能力/来源验证。
  const FamilyBirthState *births = nullptr;
};
// 输入必须是原 provenance 回调借出的真实原对象。比较原表地址和键，
// 同值拷贝不能冒原对象。返回全是借指针，不分配字符串或创建SourceRef。
// 中央唯一 registry 先validate再映射 typed owner；不得把此variant分支名
// 或物理版本当现有fact kind/schema。Birth/Common头仅沿实际头字段。
// 仅同步不可变 read lease 有效；任何根修改/候选撤回/替换即废弃全部引用。
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &,
    const CharacterOriginRecordV2 &);
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &,
    const RegistrationSourceV2 &);
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &,
    const InitialCapabilityAllocation &);
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &, const BirthFactRef &);
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &,
    const FactRecord &born_creation);
Status validate_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &,
    const CharacterAvailabilityRetentionRef &);
} // namespace hanmo::resident
