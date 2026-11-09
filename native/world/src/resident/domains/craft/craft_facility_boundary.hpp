#pragma once
#include "craft_facility_sources.hpp"
#include "craft_facility_boundary_model.hpp"
#include "resident/domains/local_economy/economic_facility_provider.hpp"
namespace hanmo::resident {
class CraftFacilityPinSink;
struct CraftFacilityBoundaryReadContext {
 const CraftFacilityOccupancyContext& occupancy;
 const EconomicFacilityProviderContext& physical;
 const FacilityClaimsCatalog& claim_catalog;
};
struct CraftFacilityBoundaryView {
 const CraftFacilityBoundarySource* original=nullptr;
 CraftFacilityBusinessSourceView owner;
 const FacilityClaim2* previous_claim=nullptr;
 CraftFacilityEligibility actual_eligibility;
};
// 原暂停只引用真实到期 episode；全 boundary/control 恰一反覆盖另在 publication 执行。
Status validate_craft_facility_until_cause(const CraftFacilityOccupancyContext&,
 const FacilitySlotRef&,const CraftFacilityClaimUntil&,const MomentOwnerSource&,const WorldMoment&);
Status visit_craft_facility_until_control_pins(
    const CraftFacilityBoundaryReadContext &, const MomentOwnerSource &,
    CraftFacilityPinSink &);
// Source-local: original cause/terms/inputs/placement and immutable claim refs.
// Does not require its own late S0 or physical-finalizer result to prove cause.
Outcome<CraftFacilityBoundaryView>
read_craft_facility_boundary_for_physical_effect(
    const CraftFacilityBoundaryReadContext &, const MomentOwnerSource &);
// 只列原 boundary 及同一次原到期的真实 S9 处置；不代表其它专业或整槽覆盖。
Outcome<std::vector<FacilitySourceRef>>
read_craft_facility_original_dispositions(
    const CraftFacilityBoundaryReadContext &, const FacilitySourceRef &,
    const FacilitySlotRef &);
// Resolves only the actual direct/merged original source for this claim.
Outcome<std::optional<CraftFacilityBoundaryView>>
read_craft_facility_boundary_for_claim(const CraftFacilityBoundaryReadContext &,
                                       const FacilityClaim2 &,
                                       const FacilityClaimMutationReceipt &);
Status validate_craft_boundary_claim_admission(const CraftFacilityBoundaryReadContext&,
 const CraftFacilityBoundaryView&,const FacilityClaim2&,const FacilityClaimMutationReceipt&);
Status validate_craft_boundary_claim_close(const CraftFacilityBoundaryReadContext&,
 const CraftFacilityBoundaryView&,const FacilityClaim2&,FacilityClaimCloseReason,const FacilityClaimMutationReceipt&);
// Original aggregate slots have no physical binding row. Independent/shared
// physical layouts must read the actual immutable binding.
Outcome<std::vector<FacilityPhysicalUnitRef>> read_craft_facility_claim_units(
 const CraftFacilityBoundaryReadContext&,const FacilityClaim2&);
// Full publication only: includes reverse original boundary/S0/physical coverage.
Status validate_craft_boundary_retention_entry(const CraftFacilityBoundaryReadContext&,
 const FacilityRetentionTransition&,const FacilityRetentionEntry&);
Status validate_craft_facility_boundaries(const CraftFacilityBoundaryReadContext&);
Outcome<std::optional<FacilityRetentionEntry>> read_craft_boundary_retention_disposition(
 const CraftFacilityBoundaryReadContext&,const FacilityClaim2&,const FacilityClaimMutationReceipt&);
Outcome<std::vector<FacilityPhysicalUnitRef>> read_craft_boundary_admission_units(
 const CraftFacilityBoundaryReadContext&,const FacilityClaim2&,const FacilityClaimMutationReceipt&);
// 准备型只反读已经保存的 S9 原采用行；不构造带伪 capacity_revision 的 Claim2。
class NativeCraftPreparedFacilityRead final:public EconomicFacilityPreparedDispositionRead {
 const CraftFacilityBoundaryReadContext& x_;
public:explicit NativeCraftPreparedFacilityRead(const CraftFacilityBoundaryReadContext&x):x_(x){}
 bool bound_to(const CommonStore&,const FacilityClaimsState&)const noexcept override;
 Status validate_prepared_facility_disposition(const FacilitySlotRef&,const PreparedFacilityClaimMutation&)const override;
 Outcome<std::optional<FacilityRetentionEntry>> read_prepared_facility_retention(const FacilityClaim2&,const PreparedFacilityClaimMutation&)const override;
 Outcome<std::vector<FacilityPhysicalUnitRef>> read_prepared_facility_admission_units(const PreparedFacilityClaimAdmission&,const PreparedFacilityClaimMutation&)const override;
};
class CraftFacilityPinSink {public:virtual ~CraftFacilityPinSink()=default;
 virtual Status source(const MomentOwnerSource&)=0;
 virtual Status claim(const StableId&,const std::optional<FacilityClaimReceiptRef>&)=0;
 virtual Status activation(const FacilityActivationReceiptRef&)=0;
};
Status visit_craft_facility_boundary_pins(const CraftFacilityBoundaryReadContext&,const MomentOwnerSource&,CraftFacilityPinSink&);
}
