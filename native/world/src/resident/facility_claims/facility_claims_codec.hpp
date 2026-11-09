// facility_claims/2严格两根和不可变历史；完整JSON/binary共用同一字段序，无active-only回填。
#pragma once
#include "model.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> encode_facility_claims2(const FacilityClaimsState&);
Outcome<FacilityClaimsState> decode_facility_claims2(const BoundaryValue&);
Status visit_facility_claims2(CodecVisitor&,FacilityClaimsState&);
Outcome<BoundaryValue> encode_facility_claims_catalog(const FacilityClaimsCatalog&);
Outcome<FacilityClaimsCatalog> decode_facility_claims_catalog(const BoundaryValue&);
Status visit_facility_slot_ref(CodecVisitor&,FacilitySlotRef&);
Status visit_facility_source_ref(CodecVisitor&,FacilitySourceRef&);
Status visit_facility_claim_receipt_ref(CodecVisitor&,FacilityClaimReceiptRef&);
}
