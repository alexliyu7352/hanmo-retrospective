// 建设 worker 的真实 S4/S1 服务输入；S0 单独拥有 Fixed 信用与暂停历史。
#pragma once
#include "facility_construction_adoption.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"

namespace hanmo::resident {
struct FacilityConstructionServiceContext {
  const FacilityConstructionAdoptionContext &adoption;
  const CharacterConditionSources &body;
  const ActorSpatialHeadSourceReadContext &spatial;
  const ActionElapsedStore &elapsed;
  const TimeScaleRef &scale;
  const MilitarySpatialHandoffState *military_handoffs = nullptr;
};
// 五个实际输入的临时拥有者；字段首次复制前收费，vector 销毁后才退还。
// 只借 values 给原 S0 prepare，不能把这个临时对象装入专业持久根。
class FacilityConstructionServiceInputs {
  CheckpointMemoryCharge copied_fields_;
  std::vector<FixedServiceInputRef> values_;

public:
  FacilityConstructionServiceInputs() = default;
  FacilityConstructionServiceInputs(const MomentOwnerSourceBuildView &start,
                                    const MomentOwnerSourceBuildView &body,
                                    const MomentOwnerSourceBuildView &presence,
                                    const CatalogIdentity &,
                                    std::string_view purpose,
                                    std::string_view rule);
  FacilityConstructionServiceInputs(const FacilityConstructionServiceInputs &) =
      delete;
  FacilityConstructionServiceInputs(
      FacilityConstructionServiceInputs &&) noexcept = default;
  FacilityConstructionServiceInputs &
  operator=(const FacilityConstructionServiceInputs &) = delete;
  FacilityConstructionServiceInputs &
  operator=(FacilityConstructionServiceInputs &&) = delete;
  const std::vector<FixedServiceInputRef> &values() const noexcept {
    return values_;
  }
};
// 当前开工捕获只同步借已核 head/位置，真正 writer 在原字段预留后保存；
// callback 不得持久保存借指针，也不等于未来服务已经完成。
class FacilityConstructionWorkerAdoptionSink {
public:
  virtual ~FacilityConstructionWorkerAdoptionSink() = default;
  virtual Status accept(const ActorConditionHead &,
                        const ActualSpatialSourceRef &,
                        const WorldMoment &) = 0;
};
// 本人必须真实空闲、未处互动/战斗，具正常工作目的能力并位于原方案地点。
// 不产生 Action、不收费，也不以未来 construction ID 伪造接受来源。
Status visit_facility_construction_adoptable_worker(
    const FacilityConstructionServiceContext &, const ActivityRead &,
    const StableId &actor_id, const EconomicRuleRef &actual_terms,
    FacilityConstructionWorkerAdoptionSink &);
// 当前采用只冻结原 head/位置/rule；不提前证明未来服务，也不发施工权限。
Outcome<FacilityConstructionServiceInputs>
read_facility_construction_current_service_inputs(
    const FacilityConstructionServiceContext &,
    const StableId &construction_id);
// 纯读唯一 S0 完整期，再逐实际服务区间核身体与位置。暂停空档不算工，
// 一个 worker 不因多个片重复累计。此口尚不证明工程权限/完成材料/应用。
Outcome<FixedServiceWorkPeriodRead2>
read_facility_construction_provided_worker_period(
    const FacilityConstructionServiceContext &, const StableId &construction_id,
    Integer period_ordinal);
} // namespace hanmo::resident
