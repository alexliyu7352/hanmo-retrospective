// Finite original personal-wallet route. Household/public/agent and successor
// continuation require their own actual authorization; this reader cannot infer it.
#pragma once
#include "repair_service.hpp"
#include "resident/domains/commerce/private_wallet_title.hpp"
namespace hanmo::resident {
struct RepairPrivateFinancialContext {
 const CommonStore&common;const ItemInstancesState&items;const LocalEconomyState&economy;
 const PrivateWalletTitleRead&titles;const CurrentMomentRead&clock;
 // Only prospective payment reads the complete balance. Historical authority
 // remains source-local so P11 can call it without a financial recursion.
 const EconomicObligationReadContext* obligations=nullptr;
};
class NativeRepairPrivateFinancialRead {
 const RepairPrivateFinancialContext&x_;
public:explicit NativeRepairPrivateFinancialRead(const RepairPrivateFinancialContext&x):x_(x){}
 bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept;
 Outcome<RepairServiceFinancialParties> read_original_repair_service_parties(const RepairServiceContractSource&)const;
 Outcome<RepairServicePaymentPermission> read_original_repair_payment_permission(const RepairServiceContractSource&,Integer,const WorldMoment&,RepairServicePaymentUse,const MomentOwnerSource& actual_responsibility)const;
 Status validate_original_repair_payment(const RepairServiceContractSource&,const MomentOwnerSource& actual_payment,RepairServicePaymentUse,Integer,const WorldMoment&)const;
};
}
