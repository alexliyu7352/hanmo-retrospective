// 原 S1 民用读者与真实版本/票据的同 World 拥有体；不制造人口或运输业务源。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "native_domain_read_versions.hpp"
#include "resident/domains/travel/native_spatial_carrier_sources.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
// 所选 group 的专业业务 owner 必须由外层同候选 lease 保活；本对象只持借用。
// 未选 group 仍实际核三拥有根与相关人口绑定为空，不以空表决定是否选择。
class NativeCivilianSpatialOwner final : public CheckpointAllocatedObject {
  const WorldBundle &world_;
  const TransportRegistrationBusinessRead *business_;
  const TransportVehicleTakeover *vehicles_;
  ReadStampLimits limits_;
  std::unique_ptr<CurrentReadStampRead> stamps_;
  NativeCivilianSpatialCarrierSources original_;
  NativeCivilianSpatialOwner(const WorldBundle &, ReadStampLimits,
                             const TransportRegistrationBusinessRead *,
                             const TransportVehicleTakeover *,
                             const MilitaryFormationSourceRead *,
                             const MilitaryReturnSourceRead *,
                             const NativeMilitaryTrainingPeriodRead *);

public:
  NativeCivilianSpatialOwner(const NativeCivilianSpatialOwner &) = delete;
  NativeCivilianSpatialOwner &
  operator=(const NativeCivilianSpatialOwner &) = delete;
  NativeCivilianSpatialOwner(NativeCivilianSpatialOwner &&) = delete;
  NativeCivilianSpatialOwner &operator=(NativeCivilianSpatialOwner &&) = delete;
  static Outcome<std::unique_ptr<NativeCivilianSpatialOwner>>
  create(const WorldBundle &, ReadStampLimits,
         const TransportRegistrationBusinessRead * = nullptr,
         const TransportVehicleTakeover * = nullptr,
         const MilitaryFormationSourceRead * = nullptr,
         const MilitaryReturnSourceRead * = nullptr,
         const NativeMilitaryTrainingPeriodRead * = nullptr);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  const NativeCivilianSpatialCarrierSources &original() const noexcept;
  const CurrentReadStampRead &stamps() const noexcept;
};
} // namespace hanmo::resident
