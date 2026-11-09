// 原S5绑定的同实例组合：不重建InputLease/Household，不引入第二资金或生产账。
#pragma once
#include "economic_exact_phase.hpp"
#include "economic_payment_world_scope.hpp"
#include "household_world_scope.hpp"
#include "market_history/economic_fifo_world_scope.hpp"
namespace hanmo::resident {
struct NativeEconomicWorldLateSources {
  std::shared_ptr<const PopulationMilitaryReadContext> military;
  std::shared_ptr<const EstateEmploymentContext> employment;
  std::shared_ptr<const BornPopulationSourceContext> born;
  std::shared_ptr<MarketBusinessCaptureServices> market;
};
class NativeEconomicWorldScope final : public CheckpointAllocatedObject {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit NativeEconomicWorldScope(std::unique_ptr<Impl>);

public:
  ~NativeEconomicWorldScope();
  NativeEconomicWorldScope(const NativeEconomicWorldScope &) = delete;
  NativeEconomicWorldScope &
  operator=(const NativeEconomicWorldScope &) = delete;
  // candidate为空只构造读lease；非空只能是传入World本体，不做const_cast。
  static Outcome<std::unique_ptr<NativeEconomicWorldScope>>
  create(const WorldBundle &, std::shared_ptr<const NativePoliticalWorldScope>,
         std::shared_ptr<const NativeEconomicPaymentWorldScope>,
         std::shared_ptr<const NativeEconomicFifoWorldScope>,
         std::shared_ptr<const NativeHouseholdWorldScope>,
         NativeEconomicWorldLateSources = {}, WorldBundle *candidate = nullptr);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  const EconomicExactReadLease &read() const noexcept;
  EconomicExactWriteLease *write() const noexcept;
};
} // namespace hanmo::resident
