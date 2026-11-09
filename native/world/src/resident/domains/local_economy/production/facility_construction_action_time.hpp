// 真实施工 Fixed 专业源；不把 S0 信用、初始产权或空接管链当许可。
#pragma once
#include "facility_construction_service_original.hpp"

namespace hanmo::resident {
const ActionTimeRegistration &facility_construction_action_time_registration();
// 借同 job 原服务根，反读真实开工、身体变更、已供期与单阶段结果。
// 当前有限分支支持本人单 worker 的原 Fixed 施工；取消/外部处分尚须
// 原 producer 闭合，未实现分支不以空结果或默认成功进入 Complete。
class NativeFacilityConstructionActionTimeRead final
    : public ActionTimeOriginalSourceRead {
  const FacilityConstructionServiceContext &x_;

public:
  explicit NativeFacilityConstructionActionTimeRead(
      const FacilityConstructionServiceContext &x)
      : x_(x) {}
  Status enumerate_action_time_owners(const ActionTimeRegistration &,
                                      ActionTimeOwnerIdSink &) const override;
  Outcome<ActionTimeOwnerProof>
  read_action_time_owner(const ActionTimeRegistration &,
                         const StableId &) const override;
  Outcome<WorldMoment>
  read_action_time_source(const ElapsedTimeSource &) const override;
  Status validate_fixed_action_sources(
      const ActionTimeRegistration &, const ActionTimeOwnerProof &,
      const ActionElapsedState &,
      const std::vector<FixedServiceTransition> &) const override;
  Status validate_fixed_action_sources_in_candidate(
      const ActionTimeRegistration &, const ActionTimeOwnerProof &,
      const ActionElapsedState &, const std::vector<FixedServiceTransition> &,
      const ActionElapsedStore &) const override;
  Status
  validate_shared_action_sources(const ActionTimeRegistration &,
                                 const ActionTimeOwnerProof &,
                                 const SharedWorkElapsedState &) const override;
  Outcome<RunStop>
  read_action_continuation_stop(const ActionTimeRegistration &,
                                const ActionTimeOwnerProof &,
                                const WorldMoment &) const override;
  Outcome<OriginalActionRemaining>
  read_original_action_remaining(const ActionTimeRegistration &,
                                 const ActionTimeOwnerProof &,
                                 const WorldMoment &) const override;
  Outcome<StableId>
  read_gate_result_id(const SharedWorkGateResult &) const override;
  Outcome<SharedWorkGateResult>
  read_original_gate_result(const StableId &) const override;
  Status
  validate_original_gate_result(const SharedWorkGateResult &) const override;
};
} // namespace hanmo::resident
