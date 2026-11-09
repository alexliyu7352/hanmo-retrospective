// Original S5 calendar and actual household-service writers in the common exact graph.
#pragma once
#include "economic_original_payments.hpp"
#include "household_service_runtime.hpp"
namespace hanmo::resident {
struct PopulationLaborHistoryContext;
struct EconomicExactReadBinding {
 const LocalEconomyCatalog& catalog;const OrganizationRead& organizations;
 const EconomicExactValidationContext& original;HouseholdServiceReadBinding household;
 const PopulationMilitaryReadContext* military=nullptr;const EconomicTakeoverReadContext* takeover=nullptr;
 const EconomicTakeoverExecutionState* takeover_execution=nullptr;const EstateEmploymentContext* employment=nullptr;
 const PrivateShopResponsibilitySourceContext* shops=nullptr;
 // 原P02/3完整履歷只借同一lease；stable/cold終端必驗，不在getter重播。
 const PopulationLaborHistoryContext *population_history = nullptr;
};
struct EconomicExactWriteBinding {
 CommonStore& common;LocalEconomyState& economy;const LocalEconomyCatalog& catalog;
 const OrganizationRead& organizations;const ActivityRead& activities;
 EconomicProductionContext& production;const EconomicCalendarReadContext& calendar;
 HouseholdServiceWriteBinding household;
 const HouseholdProvisionManagementContext* household_management=nullptr;ProductionDisruptionContext* disruption=nullptr;
 EconomicTakeoverPhaseContext* takeover=nullptr;const EstateEmploymentContext* employment=nullptr;
 MarketBusinessCaptureServices* market=nullptr;const PrivateShopResponsibilitySourceContext* shops=nullptr;
 // Borrow the same lease-owned whole-source context; selected successors require it.
 const EconomicExactValidationContext* original=nullptr;
};
class EconomicExactReadLease {public:virtual ~EconomicExactReadLease()=default;virtual EconomicExactReadBinding bindings()const=0;};
class EconomicExactWriteLease {public:virtual ~EconomicExactWriteLease()=default;virtual EconomicExactWriteBinding bindings()=0;};
// A write lease owns all adapters. The non-null phase points to this callback's
// exact frozen input and remains alive until the lease is destroyed.
// It must bind optional old modules according to the selected catalog, not silently omit them.
struct EconomicExactWorldFactory {
 Outcome<std::shared_ptr<const EconomicExactReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<EconomicExactWriteLease>> (*write)(WorldBundle&,const WorldMoment&,const DynamicStepInput*)=nullptr;
};
// Compiled central callbacks acquire the original same-world domain lease.
EconomicExactWorldFactory native_economic_exact_world_factory() noexcept;
Status initialize_economic_exact_sources_in_candidate(EconomicExactWriteBinding&);
ExactPhaseRegistration make_economic_exact_phase_registration(EconomicExactWorldFactory);
}
