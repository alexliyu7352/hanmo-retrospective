// Concrete domain-owned graph for the complete original economy. Central owns
// WorldBundle lifetime, selected-root admission and cross-domain construction.
#pragma once
#include "economic_exact_phase.hpp"
#include "born_population_sources.hpp"
#include "household_service_lease.hpp"
#include "production/production_input_lease.hpp"
#include "material_aid/material_aid.hpp"
#include "resident/domains/commerce/repair_service/repair_service.hpp"
namespace hanmo::resident {
struct NativeEconomicExactDependencies {
 NativeEconomicProductionInputDependencies production;
 NativeHouseholdServiceDependencies household;
 const OrganizationRead& organizations;
 const PopulationMilitaryReadContext& military;
 const EconomicTakeoverReadContext& takeover;
 const EconomicTakeoverExecutionState& takeover_execution;
 const EstateEmploymentContext& employment;
 const PrivateShopResponsibilitySourceContext& shops;
 // All original payment owners are required in this complete graph. A smaller
 // legacy selection must retain its original factory, not pass empty providers.
 const TransportOriginalPaymentReadContext& transport_payments;
 const GovernanceWorkExpenseReadContext& work_payments;
 const MaterialAidReadContext& aid_payments;
 const RepairServiceReadContext& repair_payments;
 const EconomicObligationReadContext& obligations;
 // The complete owner retains this context and every underlying Born root.
 // An explicitly selected successor requires it even before any birth occurs.
 Integer selected_local_economy=0;const BornPopulationSourceContext* born_population=nullptr;
};
class NativeEconomicExactSources final {
 NativeEconomicExactDependencies dependencies_;
 NativeHouseholdServiceLease household_;
 NativeEconomicProductionInputLease production_;
 NativeEconomicOriginalPaymentRead payments_;
 EconomicWorkValidationContext production_validation_;
 EconomicExactValidationContext validation_;
public:
 explicit NativeEconomicExactSources(NativeEconomicExactDependencies);
 NativeEconomicExactSources(const NativeEconomicExactSources&)=delete;
 NativeEconomicExactSources&operator=(const NativeEconomicExactSources&)=delete;
 NativeEconomicExactSources(NativeEconomicExactSources&&)=delete;
 NativeEconomicExactSources&operator=(NativeEconomicExactSources&&)=delete;
 Status validate_bindings()const;
 Status validate_sources()const;
 EconomicExactReadBinding read_binding()const;
 const NativeHouseholdServiceLease& household()const{return household_;}
 const NativeEconomicProductionInputLease& production()const{return production_;}
 const NativeEconomicOriginalPaymentRead& payments()const{return payments_;}
 const EconomicCalendarReadContext& calendar()const{return dependencies_.production.calendar;}
 const NativeEconomicExactDependencies& dependencies()const{return dependencies_;}
};
// These leases own actual concrete adapters, rather than storing prebuilt
// bindings from an arbitrary success provider. The enclosing central owner must
// retain the referenced World/context graph for the complete callback lifetime.
class NativeEconomicExactReadLease final:public EconomicExactReadLease {
 NativeEconomicExactSources sources_;
public:
 explicit NativeEconomicExactReadLease(NativeEconomicExactDependencies d):sources_(std::move(d)){}
 Status validate_bindings()const{return sources_.validate_bindings();}
 Status validate_sources()const{return sources_.validate_sources();}
 EconomicExactReadBinding bindings()const override{return sources_.read_binding();}
 const NativeEconomicExactSources& sources()const{return sources_;}
};
struct NativeEconomicExactWriteDependencies {
 NativeEconomicExactDependencies read;
 CommonStore& common;LocalEconomyState& economy;EconomicWorkSourcesState& production;
 GovernanceWorkExpenseState& work_expenses;
 HouseholdPurchaseState& purchases;ActionElapsedStore& elapsed;
 const ActivityRead& activities;
 const HouseholdProvisionManagementContext& household_management;
 ProductionDisruptionContext& disruption;
 EconomicTakeoverPhaseContext& takeover;
 MarketBusinessCaptureServices& market;
 // Exact callback input, not a fabricated replacement calendar step.
 const DynamicStepInput* phase=nullptr;
};
class NativeEconomicExactWriteLease final:public EconomicExactWriteLease {
 NativeEconomicExactWriteDependencies dependencies_;
 NativeEconomicExactSources sources_;
 EconomicProductionContext production_;
 GovernanceWorkExpenseWriter work_expenses_;
public:
 explicit NativeEconomicExactWriteLease(NativeEconomicExactWriteDependencies);
 NativeEconomicExactWriteLease(const NativeEconomicExactWriteLease&)=delete;
 NativeEconomicExactWriteLease&operator=(const NativeEconomicExactWriteLease&)=delete;
 NativeEconomicExactWriteLease(NativeEconomicExactWriteLease&&)=delete;
 NativeEconomicExactWriteLease&operator=(NativeEconomicExactWriteLease&&)=delete;
 Status validate_bindings()const;
 EconomicExactWriteBinding bindings()override;
 const NativeEconomicExactSources& sources()const{return sources_;}
 EconomicProductionContext& production_context(){return production_;}
 GovernanceWorkExpenseWriter& work_expenses(){return work_expenses_;}
};
}
