// 固定经济09：设施实体、安装、布局与滞留证据只归原物理owner；当前占用仍只归S0。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/facility_claims/facility_claims.hpp"
#include <cstdint>
namespace hanmo::resident {
using EconomicRuleRef=ActorFoundationRuleRef;
enum class EconomicFacilityAssetKind { production_asset_facility,estate_service_facility,business_service_facility };
enum class EconomicFacilitySlotKind { bed_space,furnace_slot,workbench_slot,teaching_seat };
struct FacilityActivationReceiptRef { StableId receipt_id; std::uint32_t source_schema=1; };
struct OriginalConditionBaseSlot { StableId condition_ref; EconomicRuleRef original_slot_definition_ref; };
struct InstalledServiceUnits { EconomicRuleRef installed_definition_ref; Integer units=0; };
struct ActivatedServiceUnits { FacilityActivationReceiptRef activation_receipt_ref; StableId capacity_unit_id; };
using EconomicFacilityBaseBinding=std::variant<OriginalConditionBaseSlot,InstalledServiceUnits,ActivatedServiceUnits>;
struct FacilityPhysicalUnitDefinition { StableId unit_key,capacity_unit_id,physical_position_key; std::vector<EconomicRuleRef> allowed_purpose_refs; };
struct AggregateFacilityLayout { EconomicRuleRef original_layout_rule_ref; };
struct IndependentFacilityUnits { EconomicRuleRef layout_definition_ref; std::vector<FacilityPhysicalUnitDefinition> unit_definitions; EconomicRuleRef independence_policy_ref; };
struct SharedFacilitySafetyZone { EconomicRuleRef layout_definition_ref; std::vector<FacilityPhysicalUnitDefinition> unit_definitions; EconomicRuleRef whole_zone_policy_ref; };
using EconomicFacilityPhysicalLayout=std::variant<AggregateFacilityLayout,IndependentFacilityUnits,SharedFacilitySafetyZone>;
struct EconomicFacilitySlotDefinition { EconomicRuleRef definition_ref; EconomicFacilityAssetKind asset_kind=EconomicFacilityAssetKind::production_asset_facility; EconomicFacilitySlotKind slot_kind=EconomicFacilitySlotKind::workbench_slot; StableId capacity_unit_id,physical_capacity_identity; EconomicFacilityBaseBinding base_binding; EconomicRuleRef availability_policy_ref,usage_policy_ref; Integer max_claim_half_days=0,retention_review_halfdays=0; EconomicFacilityPhysicalLayout physical_layout; };
struct FacilitySeedRowRef { StableId owner_domain; EconomicFacilityAssetKind seed_kind=EconomicFacilityAssetKind::production_asset_facility; StableId seed_row_key,seed_row_digest; };
struct FacilityInitializationAdoptionIdentity { WorldId world_id{}; StableId seed_row_key,original_asset_ref,installation_id; };
struct PhysicalFacilityInstalledEvidence { StableId installation_id; std::uint32_t source_schema=1; WorldId world_id{}; CatalogIdentity catalog_identity; FacilitySeedRowRef seed_row_ref; StableId original_asset_ref; std::optional<StableId> asset_source_key; EconomicRuleRef definition_ref; WorldMoment installed_at; FacilityInitializationAdoptionIdentity initialization_adoption_identity; };
struct InstalledPhysicalFacility { PhysicalFacilityInstalledEvidence installation_evidence; };
struct ActivatedPhysicalFacility { FacilityActivationReceiptRef original_facility_activation_receipt_ref; };
using PhysicalFacilityInitialization=std::variant<InstalledPhysicalFacility,ActivatedPhysicalFacility>;
struct PhysicalServiceFacilityExtension { StableId facility_id,asset_owner_domain,original_asset_ref; EconomicRuleRef definition_ref; StableId place_ref; PhysicalFacilityInitialization initialization_basis; };
struct InstalledPhysicalUnitSource { SourceRef source_ref; std::uint32_t source_schema=1; };
struct ActivatedPhysicalUnitSource { FacilityActivationReceiptRef original_facility_activation_receipt_ref; };
using PhysicalUnitInstallationSource=std::variant<InstalledPhysicalUnitSource,ActivatedPhysicalUnitSource>;
struct FacilityPhysicalUnitRef { StableId facility_id; PhysicalUnitInstallationSource installation_source_ref; StableId unit_key; };
struct FacilityPhysicalClaimBinding { StableId claim_id; FacilitySlotRef slot_ref; EconomicRuleRef definition_ref; std::vector<FacilityPhysicalUnitRef> physical_unit_refs; SourceRef admission_business_source_ref; std::uint32_t admission_source_schema=0; WorldMoment occurred_at; };
struct FacilityRetentionBegin { StableId origin_claim_id; FacilityClaimReceiptRef origin_admission_receipt_ref; StableId previous_claim_id,retained_claim_id; SourceRef holder_occupancy_source_ref; std::uint32_t holder_source_schema=0; MomentSpan retention_window; };
struct FacilityRetentionContinue { SourceRef previous_retention_source_ref; StableId origin_claim_id,previous_claim_id,retained_claim_id; SourceRef holder_occupancy_source_ref; std::uint32_t holder_source_schema=0; std::optional<SourceRef> clearance_source_ref; MomentSpan retention_window; };
struct FacilityRetentionClear { SourceRef previous_retention_source_ref; StableId origin_claim_id,previous_claim_id; SourceRef holder_clearance_source_ref; std::uint32_t holder_source_schema=0; };
struct FacilityRetentionRestoredService { SourceRef previous_retention_source_ref; StableId origin_claim_id,previous_claim_id,normal_claim_id; SourceRef actual_resume_result_ref; std::uint32_t actual_resume_source_schema=0; SourceRef holder_occupancy_source_ref; std::uint32_t holder_source_schema=0; };
using FacilityRetentionEntry=std::variant<FacilityRetentionBegin,FacilityRetentionContinue,FacilityRetentionClear,FacilityRetentionRestoredService>;
struct FacilityRetentionTransition { StableId transition_id; std::uint32_t source_schema=1; FacilitySlotRef slot_ref; WorldMoment occurred_at; SourceRef cause_ref; std::uint32_t cause_schema=0; std::vector<FacilityRetentionEntry> entries; std::vector<FacilitySourceRef> normal_disposition_result_refs; };
struct FacilityNotConditioned { EconomicRuleRef original_asset_definition_ref; };
struct FacilityProductionCondition { StableId condition_ref; Integer condition_revision=0; SourceRef condition_change_source_ref; };
using EconomicFacilityConditionBasis=std::variant<FacilityNotConditioned,FacilityProductionCondition>;
struct OriginalFacilityOpenRule { EconomicRuleRef rule_ref; std::optional<SourceRef> original_acceptance_ref; };
struct AcceptedFacilityOpeningIntervals { SourceRef original_opening_source_ref; std::vector<MomentSpan> intervals; };
using EconomicFacilityAvailabilityBasis=std::variant<OriginalFacilityOpenRule,AcceptedFacilityOpeningIntervals>;
struct EconomicFacilityUsageRightBasis { SourceRef original_right_ref; std::uint32_t original_right_schema=0; EconomicRuleRef usage_policy_ref; std::optional<SourceRef> original_acceptance_ref; };
struct FacilityInitializedCause { PhysicalFacilityInitialization initialization_basis; };
struct FacilityActivatedCause { FacilityActivationReceiptRef activation_receipt_ref; };
struct FacilityConditionChangedCause { SourceRef condition_change_ref; };
struct FacilityOpeningChangedCause { FacilitySourceRef opening_result_ref; };
struct FacilityRightChangedCause { FacilitySourceRef right_result_ref; };
struct FacilityRetentionChangedCause { SourceRef retention_transition_ref; };
using EconomicFacilityChangeCause=std::variant<FacilityInitializedCause,FacilityActivatedCause,FacilityConditionChangedCause,FacilityOpeningChangedCause,FacilityRightChangedCause,FacilityRetentionChangedCause>;
struct EconomicFacilityCapacitySource { StableId source_id; std::uint32_t source_schema=1; FacilitySlotRef slot_ref; Integer source_revision=0; std::optional<SourceRef> previous_source_ref; WorldMoment occurred_at; StableId facility_extension_ref; EconomicRuleRef definition_ref; EconomicFacilityBaseBinding base_basis; EconomicFacilityConditionBasis condition_basis; EconomicFacilityAvailabilityBasis availability_basis; EconomicFacilityUsageRightBasis usage_right_basis; FacilityAdmissionDisposition admission_mode; EconomicFacilityChangeCause change_cause; };
struct EconomicFacilitySourcesState { std::vector<PhysicalServiceFacilityExtension> facilities; std::vector<EconomicFacilityCapacitySource> capacity_sources; std::vector<FacilityPhysicalClaimBinding> physical_bindings; std::vector<FacilityRetentionTransition> retention_transitions; Integer schema_version=1;std::vector<EconomicFacilitySlotDefinition> definitions; };
// 只读原DTO，不存另一份claim或capacity；调用方必须绑定同一snapshot与真实holder专业reader。
struct FacilityPhysicalBindingProof { FacilityPhysicalClaimBinding binding; EconomicFacilitySlotDefinition definition; PhysicalServiceFacilityExtension facility; };
struct FacilityRetentionProof { FacilityRetentionTransition transition; std::vector<FacilityPhysicalBindingProof> physical_bindings; };
Outcome<const FacilityPhysicalClaimBinding*> read_physical_claim_binding(const EconomicFacilitySourcesState&,const StableId& claim_id);
Outcome<const FacilityRetentionTransition*> read_facility_retention_transition(const EconomicFacilitySourcesState&,const FacilitySourceRef&,const FacilitySlotRef&);
// 下列纯adapter由原物理owner实现并核完整原seed/condition/title/holder；不接受caller输入capacity或占用真假。
class EconomicFacilityOriginalSourceRead {
public:
 virtual ~EconomicFacilityOriginalSourceRead()=default;
 virtual Outcome<EconomicFacilitySlotDefinition> read_facility_slot_definition(const FacilitySlotRef&) const=0;
 virtual Outcome<EconomicFacilitySlotDefinition> read_facility_slot_definition_at_source(const FacilitySourceRef&,const FacilitySlotRef&) const=0;
 virtual Outcome<std::vector<FacilityPhysicalUnitRef>> read_physical_units_at_capacity_source(const FacilitySourceRef&,const FacilitySlotRef&) const=0;
 virtual Outcome<FacilityPhysicalBindingProof> read_physical_binding_proof(const StableId& claim_id) const=0;
 virtual Outcome<FacilityRetentionProof> read_facility_retention_proof(const FacilitySourceRef&,const FacilitySlotRef&,const MomentSpan&) const=0;
 virtual Outcome<std::vector<VerifiedFacilityCapacitySegment>> read_economic_facility_capacity_source(const FacilitySourceRef&,const FacilitySlotRef&,const MomentSpan&) const=0;
 virtual Outcome<std::vector<FacilityCapacityChange>> enumerate_economic_facility_capacity_changes(const FacilitySlotRef&,const WorldMoment&,const WorldMoment&) const=0;
};
}
