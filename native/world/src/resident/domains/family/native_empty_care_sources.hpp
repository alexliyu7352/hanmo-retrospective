// LIFE-1 明确后置出生与照护：只证明同世界原根真空，不提供任何业务能力。
#pragma once
#include "resident/domains/affairs/state_need.hpp"

namespace hanmo::resident {
struct FamilyBirthState;
struct CareRequirementsState;
struct AuthoredActorFoundationState;
struct ActionElapsedStore;
struct TravelDependentState;
struct ChildCareReliefState;
class SelectedCareGoalVisitor;

// 可空指针只表示该实际根未选择；不能用另造的空表遮盖世界中的已有根。
struct NativeEmptyCareSources {
  const CommonStore &common;
  const FamilyBirthState *births = nullptr;
  const CareRequirementsState *care = nullptr;
  const AuthoredActorFoundationState *origins = nullptr;
  const ActionElapsedStore *elapsed = nullptr;
  const TravelDependentState *dependents = nullptr;
  const ChildCareReliefState *child_care_relief = nullptr;
};

// 生命周期由持有同世界根的 scope 保证；不继承完整 Care provider 接口。
class NativeEmptyCareSourceRead final {
public:
  explicit NativeEmptyCareSourceRead(NativeEmptyCareSources sources) noexcept;
  bool bound_to(const NativeEmptyCareSources &) const noexcept;
  Status validate_sources() const;
  Outcome<StateGoalSnapshot> read_care_goal(const CareCoverageGoal &) const;
  Status visit_selected_care_goals(SelectedCareGoalVisitor &) const;
  // 请求出生或照护只能明确拒绝，不因空枚举而授权 producer。
  Status reject_birth_or_care_request() const;

private:
  NativeEmptyCareSources sources_;
};
} // namespace hanmo::resident
