// 同一实际 World 的家户 owning 图；构造只借原根，不复制业务状态或回调完整工厂。
#pragma once
#include "household_foundation_lease.hpp"
#include "resident/kernel/native_complete_world_core.hpp"

namespace hanmo::resident {
class NativeEconomicFifoRead;
// 仅已选的跨域原业务需要这些晚源。公市三口必须同时提供；未选沿唯一反向覆盖门。
// 这些真实提供者与实际 phase 均由外层 owner 保活，不允许空成功替身。
struct NativeHouseholdWorldLateSources {
  const PublicMarketStaffingOriginalRead *public_staffing = nullptr;
  const PublicMarketAuthorityRead *public_authority = nullptr;
  HouseholdRetailMarketCapture *public_market_capture = nullptr;
  const TransportRegistrationBusinessRead *transport_business = nullptr;
  const PopulationMilitaryReadContext *population_military = nullptr;
  // 别名须保活独立原 LE scope，不可回持拥有本 household 的同一父 owner。
  std::shared_ptr<const NativeEconomicFifoRead> actual_market;
};

class NativeHouseholdWorldScope final : public CheckpointAllocatedObject {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit NativeHouseholdWorldScope(std::unique_ptr<Impl>);

public:
  ~NativeHouseholdWorldScope();
  NativeHouseholdWorldScope(const NativeHouseholdWorldScope &) = delete;
  NativeHouseholdWorldScope &
  operator=(const NativeHouseholdWorldScope &) = delete;
  NativeHouseholdWorldScope(NativeHouseholdWorldScope &&) = delete;
  NativeHouseholdWorldScope &operator=(NativeHouseholdWorldScope &&) = delete;
  static Outcome<std::unique_ptr<NativeHouseholdWorldScope>>
  create(const NativeCompleteWorldCoreSources &,
         NativeHouseholdWorldLateSources = {},
         const DynamicStepInput *actual_phase = nullptr);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  // 只在中央 holder/router 全部绑定后调用；构造期间绝不读取未发布的 Complete。
  Status validate_sources() const;
  const NativeHouseholdFoundationLease &foundation() const noexcept;
  const NativeHouseholdServiceLease &service() const noexcept;
};
} // namespace hanmo::resident
