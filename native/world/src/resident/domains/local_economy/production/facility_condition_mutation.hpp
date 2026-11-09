// The original structure-effect writer's same-candidate production and S0 leg.
#pragma once
#include "production_adoption_source.hpp"
namespace hanmo::resident {
struct NativeProductionFacilityMutationContext {
 WorldBundle& world;
 CommonStore& common;
 const LocalEconomyState& economy;
 const LocalEconomyCatalog& economy_catalog;
 EconomicProductionContext& production;
 const EconomicProductionAdoptionRead& adoptions;
 EconomicFacilitySourcesState& facilities;
 EconomicFacilityReconciliationWriteContext& reconciliation;
 FacilityClaimsState& claims;
 const FacilityClaimsCatalog& claim_catalog;
 const std::vector<FacilityHolderRegistration>& holder_handlers;
 ExactPhaseWriteContext& phase_writes;
};
// Borrowed for one original effect transaction. The temporary callbacks below
// never escape this stack or become an alternative compiled-world registry.
class NativeProductionFacilityMutationServices final:public ProductionFacilityMutationServices {
 struct PreparedUnit {StableId unit_id;std::optional<PreparedProductionInputClose> close;};
 enum class Phase {idle,prepared,reconciled};
 NativeProductionFacilityMutationContext& x_;
 Phase phase_=Phase::idle;
 StableId asset_id_;
 SourceRef effect_source_,condition_source_;
 WorldMoment at_;
 std::vector<PreparedUnit> units_;
 std::vector<FacilitySlotRef> slots_;
public:
 explicit NativeProductionFacilityMutationServices(NativeProductionFacilityMutationContext& x):x_(x){}
 NativeProductionFacilityMutationServices(const NativeProductionFacilityMutationServices&)=delete;
 NativeProductionFacilityMutationServices& operator=(const NativeProductionFacilityMutationServices&)=delete;
 Status close_facility_production_inputs(const StableId&,const WorldMoment&,const SourceRef&) override;
 Status reconcile_facility_capacity_change(FacilityConditionChange&) override;
 Status reopen_facility_production_inputs(const StableId&,const WorldMoment&) override;
};
// The actual native entry supplies these services to the one original writer.
Outcome<StableId> apply_native_production_facility_effect_in_candidate(
 NativeProductionFacilityMutationContext&,const ProductionFacilityEffectRead&,
 const StableId& condition_ref,Integer expected_revision,const FacilityConditionEffectSource&);
// These use the retained condition payload and actual original holder outputs.
// With no claims they append capacity only, never an empty shared mutation.
Status validate_native_facility_condition_reconciliation(
 const NativeProductionFacilityMutationContext&,const FacilityCapacityReconciliationRequest&);
Outcome<std::optional<FacilityClaimMutationReceipt>> finalize_native_facility_condition_reconciliation_in_candidate(
 NativeProductionFacilityMutationContext&,const FacilityCapacityReconciliationRequest&,
 const std::vector<FacilityClaimMutationReceipt>&);
// Complete reverse coverage of the actual condition -> capacity -> S0 legs.
Status validate_production_facility_capacity_adoptions(const EconomicFacilityProviderContext&);
}
