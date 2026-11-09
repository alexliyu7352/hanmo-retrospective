// 二进制直接遍历当前独占深值，JSON沿同一字段访问；解码后仍须原owner来源闭包校验。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "facility_claims_codec.hpp"
#include "facility_claims_codec_fields.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include <optional>
namespace hanmo::resident {
Outcome<BoundaryValue> encode_facility_claims2(const FacilityClaimsState& v){return exact_fields::encode_json(v,[](auto& c,auto& x){return facility_fields::state(c,x);});}
Outcome<FacilityClaimsState> decode_facility_claims2(const BoundaryValue& v){return exact_fields::decode_json<FacilityClaimsState>(v,[](auto& c,auto& x){return facility_fields::state(c,x);});}
Status visit_facility_claims2(CodecVisitor& c,FacilityClaimsState& v){return exact_fields::visit_binary(c,v,[](auto& x,auto& value){return facility_fields::state(x,value);});}
Outcome<BoundaryValue> encode_facility_claims_catalog(const FacilityClaimsCatalog& v){return exact_fields::encode_json(v,[](auto& c,auto& x){return facility_fields::catalog(c,x);});}
// 原生冷目录借当前候选的同一任务；独立非任务解码保持原行为，缺根仍由原读者拒绝。
Outcome<FacilityClaimsCatalog>
decode_facility_claims_catalog(const BoundaryValue &v) {
  std::optional<ScopedCheckpointJsonDecode> scope;
  if (ScopedCheckpointBoundaryDecode::current())
    scope.emplace(CheckpointCatalogJsonKind::facility_claims);
  return exact_fields::decode_json<FacilityClaimsCatalog>(
      v, [](auto &c, auto &x) { return facility_fields::catalog(c, x); });
}
Status visit_facility_slot_ref(CodecVisitor &c, FacilitySlotRef &v) {
  return exact_fields::visit_binary(c, v, [](auto &x, auto &value) {
    return facility_fields::slot(x, value);
  });
}
Status visit_facility_source_ref(CodecVisitor &c, FacilitySourceRef &v) {
  return exact_fields::visit_binary(c, v, [](auto &x, auto &value) {
    return facility_fields::source_ref(x, value);
  });
}
Status visit_facility_claim_receipt_ref(CodecVisitor& c,FacilityClaimReceiptRef& v){return exact_fields::visit_binary(c,v,[](auto& x,auto& value){return facility_fields::receipt_ref(x,value);});}
}
