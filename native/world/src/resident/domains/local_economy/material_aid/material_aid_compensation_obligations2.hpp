#pragma once
#include "material_aid_compensation2.hpp"
namespace hanmo::resident {
// Owns no alternative world/amount ledger; borrows the same selected /2 graph.
class NativeMaterialAidCompensationOriginalRead2 final:public EconomicObligationOriginalRead {
 public:NativeMaterialAidCompensationOriginalRead2(const MaterialAidCompensationReadContext2&x,const BudgetAuthorizationRead&a,const EconomicObligationOriginalRead&r):x_(x),authorization_(a),remainder_(r){}
 bool bound_to(const CommonStore&,const LocalEconomyState&)const noexcept override;
 Outcome<OriginalEconomicPayable>read_original_payable(const EconomicObligationOriginalSource&)const override;
 Outcome<OriginalEconomicAdjustment>read_original_adjustment(const MomentOwnerSource&)const override;
 Outcome<OriginalEconomicCashPayment>read_original_cash_payment(const SourceRef&)const override;
 Status validate_original_cash_payment_authority(const SourceRef&)const override;
 Outcome<ObligationResponsibilityEpoch>read_original_responsibility_genesis(const EconomicObligationOriginalSource&)const override;
 Status validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&)const override;
 private:const MaterialAidCompensationReadContext2&x_;const BudgetAuthorizationRead&authorization_;const EconomicObligationOriginalRead&remainder_;
};
}
