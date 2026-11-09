// Shared S5 execution for original service contracts. No contract, balance or
// allocation table is introduced here. Domain adapters still validate their
// accepted terms, actual earning/termination source and payment permission.
#pragma once
#include "local_economy_model.hpp"
#include "obligations/obligation_sources_read.hpp"
#include "resident/storage/resource_execution_draft.hpp"
namespace hanmo::resident::service_finance {
// Original owner first publishes its actual charge/refund source in this same
// candidate. IDs, amount, exact dates and epoch0 parties come only from that
// source. The old caller-assembled fake recurring obligation entry is removed.
Status post_payable_in_candidate(EconomicObligationWriteContext&,
 const EconomicObligationOriginalSource&);
// Exact same-world bridges used by the original source owners. A missing P11
// context is a selected capability error, not a fallback to the old debt math.
Outcome<const EconomicObligationReadContext*> original_obligation_read(const CommonStore&,
 const LocalEconomyState&,const EconomicObligationReadContext*);
Outcome<EconomicObligationWriteContext*> original_obligation_write(CommonStore&,
 LocalEconomyState&,const EconomicObligationReadContext*,EconomicObligationWriteContext*);
class AppliedMoney {
 const CommonStore* common_=nullptr;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_;
 std::size_t draft_=0,payer_operation_=0;
 bool consumed_=false;
 friend Outcome<AppliedMoney> transfer_money_in_candidate(CommonStore&,const StableId&,const StableId&,Integer,const SetReservation*);
 friend Status record_payment_in_candidate(EconomicObligationWriteContext&,const PaymentReceipt&,AppliedMoney&&);
public:
 Integer payer_before=0,payer_after=0,payee_before=0,payee_after=0;
 AppliedMoney()=default;AppliedMoney(const AppliedMoney&)=delete;AppliedMoney&operator=(const AppliedMoney&)=delete;
 AppliedMoney(AppliedMoney&&)=default;AppliedMoney&operator=(AppliedMoney&&)=default;
};
// Shared recipe construction also used by the original merchant transaction.
// Zero is permitted for the already supported zero-price original trade.
Status append_money_transfer_recipe(const CommonStore&,ResourceRecipe&,
 const StableId& payer,const StableId& payee,Integer amount);
// Only the original optional reservation update and two Common WalletDelta
// legs. The reservation remains in this same atomic ResourceRecipe.
Outcome<AppliedMoney> transfer_money_in_candidate(CommonStore&,const StableId& payer,
 const StableId& payee,Integer amount,const SetReservation* original_reservation=nullptr);
// Consumes the same-candidate captured transfer once, then records the sole
// original LE PaymentReceipt and credits the original arrear. It cannot mint a
// payment from a caller-provided balance or from a mere amount.
Status record_payment_in_candidate(EconomicObligationWriteContext&,
 const PaymentReceipt&,AppliedMoney&&);
}
