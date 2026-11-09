#pragma once
#include "../material_aid/material_aid.hpp"
namespace hanmo::resident {
// Original P27 establishment / actual receiving source leg of the sole P11
// router. Other domains remain owned by the real same-world remainder reader.
// No copy of money, paid amounts, current balances or aid schedules is stored.
class NativeMaterialAidObligationOriginalRead final:public EconomicObligationOriginalRead {
 const MaterialAidReadContext&x_;const EconomicObligationOriginalRead&remainder_;
public:
 NativeMaterialAidObligationOriginalRead(const MaterialAidReadContext&x,
  const EconomicObligationOriginalRead&r):x_(x),remainder_(r){}
 bool bound_to(const CommonStore&,const LocalEconomyState&)const noexcept override;
 Outcome<OriginalEconomicPayable>read_original_payable(const EconomicObligationOriginalSource&)const override;
 Outcome<OriginalEconomicAdjustment>read_original_adjustment(const MomentOwnerSource&)const override;
 Outcome<OriginalEconomicCashPayment>read_original_cash_payment(const SourceRef&)const override;
 Status validate_original_cash_payment_authority(const SourceRef&)const override;
 Outcome<ObligationResponsibilityEpoch>read_original_responsibility_genesis(const EconomicObligationOriginalSource&)const override;
 Status validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&)const override;
};
}
