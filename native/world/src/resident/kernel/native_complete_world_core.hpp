// 唯一 Complete 同世界借用图的轻声明；不复制来源，不拉专业构造或 Field 头。
#pragma once
namespace hanmo::resident {
struct WorldBundle;
class CompleteExactWorldBindingOwner;
class CurrentMomentRead;
class OriginalWorldReadBoundary;
class FieldSegmentApplicationRead;
class CharacterAttributeAbsentSourcesReadV1;
struct MilitaryAccess;
class MilitaryFormationSourceRead;
class MilitaryReturnSourceRead;
class NativeMilitaryTrainingPeriodRead;
class MilitaryActionPlacementRead;
struct GrowthFixedSpatialSources;
class NativeCharacterFamilyWorldRead;
class NativeInteractionWorldRead;
class NativeOriginalSocialWorldRead;
class NativeNamedMilitaryCustodyWorldRead;
class ActionTimeOriginalSourceRead;
class FacilityHolderSourceRead;
class TransportRegistrationBusinessRead;
struct NativeOrganizationResourceValidationReads;
struct NativeCompleteWorldEarlySources {
  const WorldBundle &world;
  const CompleteExactWorldBindingOwner &complete;
  const CurrentMomentRead &clock;
  const MilitaryAccess &military_access;
  const MilitaryFormationSourceRead *formations = nullptr;
  const MilitaryReturnSourceRead *returns = nullptr;
  const NativeMilitaryTrainingPeriodRead *periods = nullptr;
  const MilitaryActionPlacementRead *military_placements = nullptr;
  // Both are owned before Family/Body by the same unmovable complete lease.
  const OriginalWorldReadBoundary *original_read_boundary = nullptr;
  const FieldSegmentApplicationRead *field_application_clock = nullptr;
  const CharacterAttributeAbsentSourcesReadV1 *attribute_absence = nullptr;
  // 同一中央图先持转发地址；只有真 Household 业务构成并绑定后才能发布或读取。
  const TransportRegistrationBusinessRead *transport_business = nullptr;
};
struct NativeCompleteWorldCharacterSources {
  const NativeCompleteWorldEarlySources &early;
  const GrowthFixedSpatialSources &spatial;
  const NativeCharacterFamilyWorldRead &family;
  const NativeInteractionWorldRead *interaction = nullptr;
  const NativeOriginalSocialWorldRead *social = nullptr;
};
struct NativeCompleteWorldCoreSources {
  const NativeCompleteWorldCharacterSources &characters;
  const ActionTimeOriginalSourceRead &action_time;
  const FacilityHolderSourceRead &facility_holders;
  // 原唯一资源闭包由同一不移动 outer owner 持有，不授新权限。
  const NativeOrganizationResourceValidationReads &organization_resources;
  // 原同 World 完整看管 owner，只在私有构造完成后绑定，不是持久状态。
  const NativeNamedMilitaryCustodyWorldRead *named_custody = nullptr;
};
} // namespace hanmo::resident
