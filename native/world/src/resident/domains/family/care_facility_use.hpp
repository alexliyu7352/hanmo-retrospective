// FH 4.1 / 7.2 / 8: Care's local immutable FacilityUseBinding/1.
// These rows belong only to care_requirements/5; S0 still owns every claim.
#pragma once
#include "care_arrangement.hpp"
#include "resident/facility_claims/model.hpp"
namespace hanmo::resident {
struct CareArrangementRef {StableId accepted_arrangement_id;};
struct CareFacilityOwnClaim {StableId original_claim_id;ReservationRef holder;};
struct CareFacilityBorrowedPhysicalHolder {
 SourceRef physical_holder_source_ref;Integer physical_holder_source_schema=0;
 StableId placement_claim_id;SourceRef authorized_use_source_ref;
 Integer authorized_use_source_schema=0;
};
using CareFacilityUseMode=std::variant<CareFacilityOwnClaim,CareFacilityBorrowedPhysicalHolder>;
struct CareFacilityUseBinding {
 StableId binding_id;CareArrangementRef owner_business_ref;StableId accepted_terms_ref;
 ActorFoundationRuleRef usage_rule_ref;FacilitySlotRef slot_ref;CareFacilityUseMode mode;
 SourceRef created_source_ref;Integer created_source_schema=0;WorldMoment created_at;
 FieldOrder field_order;
};
// A borrowed projection of one actual professional adoption. The concrete
// owner must read the actual accepted rule/slot, original claim or body-holder
// and authorization. It must not call this binding's whole validator.
struct CareFacilityOriginalUse {
 const AcceptedCaregiver* accepted=nullptr;
 ActorFoundationRuleRef usage_rule_ref;FacilitySlotRef slot_ref;
 StableId physical_capacity_identity;CareFacilityUseMode mode;
 MomentOwnerSource actual_created_source;WorldMoment created_at;
};
class CareFacilityUseAdoptionVisitor {
public:virtual ~CareFacilityUseAdoptionVisitor()=default;
 virtual Status adopted(const CareArrangementRef&,const FacilitySlotRef&,
  const MomentOwnerSource& actual_created_source)=0;
};
class CareFacilityUseOriginalRead {
public:virtual ~CareFacilityUseOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const FacilityClaimsState&)const noexcept=0;
 // This also proves that a bed really is required by the original Care terms,
 // and that an existing S7/S10 holder is borrowed rather than claimed again.
 virtual Outcome<CareFacilityOriginalUse> read_original_care_facility_use(
  const CareArrangementRef&,const FacilitySlotRef&,const MomentOwnerSource&)const=0;
 // Enumerate actual adopted required slots, including those whose binding is
 // missing. A scan of facility_use_bindings is not original reverse coverage.
 virtual Status visit_original_care_facility_use_adoptions(CareFacilityUseAdoptionVisitor&)const=0;
};
struct CareFacilityUseSources {
 const CareAcceptanceContext& acceptance;const FacilityClaimsState& claims;
 const FacilityClaimsCatalog& claim_catalog;
 const OrderedTable<CareFacilityUseBinding>& bindings;
 const CareFacilityUseOriginalRead& originals;
};
SourceRef care_facility_use_source(const StableId&);
Outcome<Integer> care_facility_use_source_schema(const SourceRef&);
Status validate_care_facility_use_shape(const CareFacilityUseBinding&,Integer selected_care_version);
Status validate_care_facility_use_sources(const CareFacilityUseSources&);
Outcome<const CareFacilityUseBinding*> read_care_facility_use_binding(
 const CareFacilityUseSources&,const MomentOwnerSource&);
struct CareFacilityUseApplication {MomentOwnerSource binding_ref;bool already_applied=false;};
// Called only after the real original adoption in the same private candidate.
// It performs no reservation, physical placement, fee or claim close.
Outcome<CareFacilityUseApplication> bind_care_facility_use_in_candidate(
 CommonStore&,OrderedTable<CareFacilityUseBinding>&,const CareFacilityUseSources&,
 const CareArrangementRef&,const FacilitySlotRef&,const MomentOwnerSource& actual_created_source);
}
