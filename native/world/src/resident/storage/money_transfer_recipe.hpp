// Original Common cash recipes only. These builders do not authorize payment,
// execute resources, reserve new funds, or record a debt/payment result.
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident::resource_recipe {
// Finite original WalletDelta policies. Keep the original domain's source and
// reservation validation. Common checks the actual sequential wallet images.
enum class MoneyTransferPolicy { spendable, original_balance, spendable_debit_original_credit, original_debit_spendable_credit };
enum class ReservedMoneyPaymentOrder {
    release_then_spendable,       // original tactic lesson
    release_then_original_balance, // original military reserved charge
    debit_then_release           // original Study: payer false, recipient true
};
// Borrowed, ordered original primitive legs. A cold consumer may compare the
// actual captured requests without allocating another recipe or executing cash.
class MoneyTransferLegSink {
public:
 virtual ~MoneyTransferLegSink()=default;
 virtual Status wallet_delta(const StableId& wallet_id,Integer delta,bool require_spendable)=0;
};
Status visit_original_money_transfer(const StableId& payer,const StableId& payee,
 Integer amount,MoneyTransferPolicy,MoneyTransferLegSink&);
Status append_money_transfer(ResourceRecipe&,const StableId& payer,
 const StableId& payee,Integer amount,MoneyTransferPolicy);
Status append_reserved_money_payment(ResourceRecipe&,const SetReservation& original_update,
 const StableId& payee,Integer amount,ReservedMoneyPaymentOrder);
// Distinct parties/wallets, where required, are original domain rules.
// No current-wallet lookup here: the same original recipe can contain a prior
// CreateWallet. Nothing is applied until Common validates/applies that recipe.
}
