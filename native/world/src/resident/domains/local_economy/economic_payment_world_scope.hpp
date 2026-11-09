// P1有限原图的付款来源拥有口；只沿真实选择省去未选分支，不另造零付款provider。
#pragma once
#include "economic_original_payments.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class NativePoliticalWorldScope;
struct NativeEconomicPaymentWorldSources {
  std::shared_ptr<const NativePoliticalWorldScope> political;
  std::shared_ptr<const TransportOriginalPaymentReadContext> transport;
  std::shared_ptr<const GovernanceWorkExpenseReadContext> governance;
  std::shared_ptr<const RepairServiceReadContext> repair;
  std::shared_ptr<const EconomicObligationReadContext> obligations;
};
class NativeEconomicPaymentWorldScope final : public CheckpointAllocatedObject {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit NativeEconomicPaymentWorldScope(std::unique_ptr<Impl>);

public:
  ~NativeEconomicPaymentWorldScope();
  NativeEconomicPaymentWorldScope(const NativeEconomicPaymentWorldScope &) =
      delete;
  NativeEconomicPaymentWorldScope &
  operator=(const NativeEconomicPaymentWorldScope &) = delete;
  static Outcome<std::unique_ptr<NativeEconomicPaymentWorldScope>>
  create(const WorldBundle &, NativeEconomicPaymentWorldSources);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  const NativeEconomicOriginalPaymentRead &read() const noexcept;
  // 同一P11原根/原clock；未选才为空，内部拥有或真实外借均不复制账本。
  const EconomicObligationReadContext *obligations() const noexcept;
};
} // namespace hanmo::resident
