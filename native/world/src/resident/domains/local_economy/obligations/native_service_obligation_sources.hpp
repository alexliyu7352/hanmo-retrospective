#pragma once
#include "original_service_payable.hpp"
namespace hanmo::resident {
// Finite source router for the two actually implemented original fee owners.
// Other P11 origins require their own real adapters before full admission.
class NativeServiceObligationOriginalRead final
    : public EconomicObligationOriginalRead {
  const CommonStore &common_;
  const LocalEconomyState &economy_;
  const GovernanceWorkExpenseReadContext *work_;
  const RepairServiceReadContext *repair_;

public:
  // 空指针只由拥有工厂在真实模块未选且原账全反覆盖后使用。
  NativeServiceObligationOriginalRead(const CommonStore &,
                                      const LocalEconomyState &,
                                      const GovernanceWorkExpenseReadContext *,
                                      const RepairServiceReadContext *);
  NativeServiceObligationOriginalRead(const GovernanceWorkExpenseReadContext &,
                                      const RepairServiceReadContext &);
  bool bound_to(const CommonStore &,
                const LocalEconomyState &) const noexcept override;
  Outcome<OriginalEconomicPayable> read_original_payable(
      const EconomicObligationOriginalSource &) const override;
  Outcome<OriginalEconomicAdjustment>
  read_original_adjustment(const MomentOwnerSource &) const override;
  Outcome<OriginalEconomicCashPayment>
  read_original_cash_payment(const SourceRef &) const override;
  Status
  validate_original_cash_payment_authority(const SourceRef &) const override;
  Outcome<ObligationResponsibilityEpoch> read_original_responsibility_genesis(
      const EconomicObligationOriginalSource &) const override;
  Status validate_original_responsibility_epoch(
      const ObligationResponsibilityEpoch &) const override;
};
} // namespace hanmo::resident
