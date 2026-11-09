#pragma once
#include "../economic_transport_sources.hpp"
namespace hanmo::resident {
// Source-local original transport leg of the same P11 router. The remainder
// owns other original domains; it must be bound to these exact Common/LE roots.
// No transport price, outstanding balance or responsibility table is copied.
class NativeTransportObligationOriginalRead final:public EconomicObligationOriginalRead {
 const CommonStore&common_;const LocalEconomyState&economy_;
 const EconomicTransportExpenseState&transport_;const EconomicTransportPaymentContext&context_;
 const EconomicObligationOriginalRead&remainder_;
public:
 NativeTransportObligationOriginalRead(const CommonStore&c,const LocalEconomyState&e,
  const EconomicTransportExpenseState&t,const EconomicTransportPaymentContext&x,
  const EconomicObligationOriginalRead&r):common_(c),economy_(e),transport_(t),context_(x),remainder_(r){}
 bool bound_to(const CommonStore&,const LocalEconomyState&)const noexcept override;
 Outcome<OriginalEconomicPayable>read_original_payable(const EconomicObligationOriginalSource&)const override;
 Outcome<OriginalEconomicAdjustment>read_original_adjustment(const MomentOwnerSource&)const override;
 Outcome<OriginalEconomicCashPayment>read_original_cash_payment(const SourceRef&)const override;
 Status validate_original_cash_payment_authority(const SourceRef&)const override;
 Outcome<ObligationResponsibilityEpoch>read_original_responsibility_genesis(const EconomicObligationOriginalSource&)const override;
 Status validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&)const override;
};
}
