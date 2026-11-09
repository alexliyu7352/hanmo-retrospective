#pragma once
#include "market_service_capacity.hpp"
#include "named_household_service.hpp"
namespace hanmo::resident {
// 原变化已成立，但容量后继尚未登记。逐claim沿S0给出的全slot优先序核未来整窗。
Status validate_market_service_reconciliation_request(const MarketServiceCapacityContext&,const FacilityCapacityReconciliationRequest&);
Outcome<bool> can_continue_market_service_claim(const MarketServiceCapacityContext&,const FacilityCapacityReconciliationRequest&,const FacilityReconciliationProgress&);
// Cold replay of the original all-slot priority decision and the one actual
// capacity adoption receipt. No current capacity or self-asserted close proves it.
Status validate_household_market_service_interruption(const MarketServiceCapacityContext&,
 const StableId& purchase_id,const SourceRef& actual_cause,const WorldMoment&);
// Named只写原事件/elapsed与关闭证明；S0最后统一采用同槽receipt。
Outcome<FacilityClaimMutationReceipt> reconcile_named_household_market_service_in_candidate(CommonStore&,HouseholdPurchaseState&,ActionElapsedStore&,const HouseholdPurchaseContext&,const MarketServiceCapacityContext&,const FacilityCapacityReconciliationRequest&,const FacilityReconciliationProgress&);
Outcome<FacilityClaimMutationReceipt> reconcile_household_market_service_in_candidate(CommonStore&,HouseholdPurchaseState&,ActionElapsedStore&,const HouseholdPurchaseContext&,const MarketServiceCapacityContext&,const FacilityCapacityReconciliationRequest&,const FacilityReconciliationProgress&);
// 所有真实owner处置完成后，登记唯一容量后继并晚绑原cause；无claim不造空mutation。
Outcome<std::optional<FacilityClaimMutationReceipt>> finalize_market_service_capacity_in_candidate(CommonStore&,HouseholdPurchaseState&,const MarketServiceCapacityContext&,const FacilityCapacityReconciliationRequest&,const std::vector<FacilityClaimMutationReceipt>&);
}
