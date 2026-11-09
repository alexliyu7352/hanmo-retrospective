// 同一 World 的有限属性来源否定证明；不保存属性、不提供可伪造的中性开关。
#pragma once
#include "resident/domains/actor_foundation/effective_attributes_v1.hpp"

namespace hanmo::resident {
struct WorldBundle;

// 生命周期由原 Complete/Field/Personal 读取栈持有，不能借另一个候选的目录或根。
class NativeCharacterAttributeAbsenceRead final
    : public CharacterAttributeAbsentSourcesReadV1 {
public:
  explicit NativeCharacterAttributeAbsenceRead(
      const WorldBundle &world) noexcept;
  NativeCharacterAttributeAbsenceRead(
      const NativeCharacterAttributeAbsenceRead &) = delete;
  NativeCharacterAttributeAbsenceRead &
  operator=(const NativeCharacterAttributeAbsenceRead &) = delete;

  bool bound_to(const CommonStore &, const AuthoredActorFoundationState &,
                const ActorFoundationCatalog &) const noexcept override;
  Status validate_finite_attribute_source_absence(
      const WorldMoment &actual_now) const override;

private:
  const WorldBundle &world_;
};
} // namespace hanmo::resident
