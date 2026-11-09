#pragma once
#include "economic_facility_provider.hpp"
#include <cstddef>
#include <string_view>

namespace hanmo::resident {
// Borrowed field/element coordinates within ONE actual typed consumer. A central
// adapter retains the real consumer identity and normalizes these coordinates;
// no domain-local owning pin vector or fabricated source address is required.
struct EconomicFacilityPinPath {
 std::string_view field;
 std::size_t element=0,nested_element=0;
};
class EconomicFacilitySourcePinSink {
public:
 virtual ~EconomicFacilitySourcePinSink()=default;
 virtual Status source(EconomicFacilityPinPath,const SourceRef&,Integer actual_schema)=0;
 // These original fields have no stored schema. The adapter MUST resolve the
 // genuine selected source owner; this is not permission to assume schema1.
 virtual Status source_requiring_original_schema(EconomicFacilityPinPath,const SourceRef&)=0;
 virtual Status rule(EconomicFacilityPinPath,const EconomicRuleRef&)=0;
 virtual Status catalog(EconomicFacilityPinPath,const CatalogIdentity&)=0;
 virtual Status seed(EconomicFacilityPinPath,const FacilitySeedRowRef&,const CatalogIdentity&)=0;
 virtual Status activation(EconomicFacilityPinPath,const FacilityActivationReceiptRef&)=0;
 virtual Status extension(EconomicFacilityPinPath,const PhysicalServiceFacilityExtension&)=0;
 // The original inline installation address belongs to this actual extension.
 virtual Status installation(EconomicFacilityPinPath,const PhysicalServiceFacilityExtension&)=0;
 virtual Status condition_seed(EconomicFacilityPinPath,const StableId& condition_id)=0;
 virtual Status condition_revision(EconomicFacilityPinPath,const StableId& condition_id,Integer actual_revision)=0;
 virtual Status claim(EconomicFacilityPinPath,const StableId& actual_claim_id)=0;
 virtual Status claim_receipt(EconomicFacilityPinPath,const FacilityClaimReceiptRef&)=0;
 virtual Status physical_binding(EconomicFacilityPinPath,const FacilityPhysicalClaimBinding&)=0;
};

// Direct edges only. Each row must be the borrowed actual member of the given
// original root; copied/caller-created records reject. These identity gates do
// not run whole reverse coverage recursively, confer authority, or replace the
// original complete Facility/P08/holder validation before admission.
Status visit_economic_facility_definition_pins(const EconomicFacilitySourcesState&,const EconomicFacilitySlotDefinition&,EconomicFacilitySourcePinSink&);
Status visit_economic_facility_extension_pins(const EconomicFacilitySourcesState&,const PhysicalServiceFacilityExtension&,EconomicFacilitySourcePinSink&);
Status visit_economic_facility_installation_pins(const EconomicFacilitySourcesState&,const PhysicalServiceFacilityExtension&,EconomicFacilitySourcePinSink&);
Status visit_economic_facility_capacity_pins(const EconomicFacilitySourcesState&,const EconomicFacilityCapacitySource&,EconomicFacilitySourcePinSink&);
Status visit_economic_facility_binding_pins(const EconomicFacilitySourcesState&,const FacilityPhysicalClaimBinding&,EconomicFacilitySourcePinSink&);
Status visit_economic_facility_retention_pins(const EconomicFacilitySourcesState&,const FacilityRetentionTransition&,EconomicFacilitySourcePinSink&);
Status visit_production_facility_condition_pins(const ProductionFacilityConditionsState&,const ProductionFacilityConditionsCatalog&,const EconomicFacilitySourcesState&,const ProductionFacilityCondition&,EconomicFacilitySourcePinSink&);
Status visit_production_facility_condition_change_pins(const CommonStore&,const ProductionFacilityConditionsState&,const ProductionFacilityConditionsCatalog&,const EconomicFacilitySourcesState&,const FacilityConditionChange&,EconomicFacilitySourcePinSink&);
}
