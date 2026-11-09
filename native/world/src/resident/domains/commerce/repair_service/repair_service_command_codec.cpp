#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "repair_service_command_model.hpp"
#include "repair_service_codec_archive.hpp"
#include "resident/domains/craft/repair_acceptance.hpp"
namespace hanmo::resident::repair_service_fields {
template<class C>void fields(C&c,OfferRepairServiceRequest&v){c("terms",v.terms);c("inspection",v.inspection);c("actual_contact_source",v.actual_contact_source);}
template<class C>void fields(C&c,AcceptRepairServiceOfferRequest&v){c("offer_ref",v.offer_ref);c("actual_contact_source",v.actual_contact_source);}
template<class C>void fields(C&c,DeliverRepairItemRequest&v){c("contract_ref",v.contract_ref);c("actual_item_selection",v.actual_item_selection);c("actual_contact_source",v.actual_contact_source);}
template<class C>void fields(C&c,ReceiveRepairItemRequest&v){c("contract_ref",v.contract_ref);c("actual_destination_inventory",v.actual_destination_inventory);c("actual_contact_source",v.actual_contact_source);}
template<class C>void fields(C&c,RepairServiceCommandReceipt&v){c("command_kind",v.command_kind);c("actor_id",v.actor_id);c("actual_source",v.actual_source);c("occurred_at",v.occurred_at);c("accepted_result",v.accepted_result);}
}
namespace hanmo::resident {
Status validate_repair_service_command_receipt_shape(const RepairServiceCommandReceipt&v){
 auto invalid=[&](const char*code){return common_failure(code,v.actual_source.source_ref.id,ErrorKind::internal);};
 if(v.actor_id.empty()||v.occurred_at.half_day_tick<0||v.actual_source.source_schema!=1||!valid_source_ref(v.actual_source.source_ref))return invalid("InvalidRepairCommandReceipt");
 // The exact numeric owner checks the canonical offset; shape validation never encodes a wire value.
 auto at=compare_world_moments(v.occurred_at,WorldMoment{});if(!at.ok)return {false,{},at.error};
 if(v.command_kind=="offer_repair_service"){
  if(v.accepted_result||!same_source_ref(v.actual_source.source_ref,repair_service_offer_source(v.actual_source.source_ref.id)))return invalid("RepairOfferReceiptShapeMismatch");
 }else if(v.command_kind=="accept_repair_service_offer"){
  if(!v.accepted_result||!same_source_ref(v.actual_source.source_ref,repair_service_assent_source(v.actual_source.source_ref.id)))return invalid("RepairAssentReceiptShapeMismatch");
  const auto&r=*v.accepted_result;
  if(r.commercial_contract.source_schema!=1||r.repair_acceptance.source_schema!=1||!valid_source_ref(r.commercial_contract.source_ref)||!valid_source_ref(r.repair_acceptance.source_ref)||!same_source_ref(r.commercial_contract.source_ref,repair_service_contract_source(r.commercial_contract.source_ref.id))||!same_source_ref(r.repair_acceptance.source_ref,repair_accepted_source(r.repair_acceptance.source_ref.id))||r.commercial_contract.source_ref.id==r.repair_acceptance.source_ref.id||r.commercial_contract.source_ref.id==v.actual_source.source_ref.id||r.repair_acceptance.source_ref.id==v.actual_source.source_ref.id)return invalid("RepairAcceptedResultShapeMismatch");
 }else if(v.command_kind=="deliver_repair_item"){
  if(v.accepted_result||!same_source_ref(v.actual_source.source_ref,repair_custody_received_source(v.actual_source.source_ref.id)))return invalid("RepairDeliveryReceiptShapeMismatch");
 }else if(v.command_kind=="receive_repair_item"){
  if(v.accepted_result||!same_source_ref(v.actual_source.source_ref,repair_item_returned_source(v.actual_source.source_ref.id)))return invalid("RepairReturnReceiptShapeMismatch");
 }else return common_failure("UnsupportedRepairServiceCommand",v.command_kind,ErrorKind::unsupported_capability);
 return common_success();
}
// 依命令原字段解码请求；字段、类型或覆盖校验失败时返回原错误。
Outcome<RepairServiceCommandRequest>
decode_repair_service_command_request(const StableId &kind,
                                      const BoundaryValue &v) {
  try {
    if (kind == "offer_repair_service") {
      OfferRepairServiceRequest out;
      ActorFoundationJsonVisitor archive(v);
      craft_codec::checked(archive.defer_duplicate_field_check_to_finish());
      repair_service_codec::Decode f{archive, v, "/offer_repair_service"};
      repair_service_fields::fields(f, out);
      f.finish();
      return {true, std::move(out), {}};
    }
    if (kind == "accept_repair_service_offer") {
      AcceptRepairServiceOfferRequest out;
      ActorFoundationJsonVisitor archive(v);
      craft_codec::checked(archive.defer_duplicate_field_check_to_finish());
      repair_service_codec::Decode f{archive, v,
                                     "/accept_repair_service_offer"};
      repair_service_fields::fields(f, out);
      f.finish();
      return {true, std::move(out), {}};
    }
    if (kind == "deliver_repair_item") {
      DeliverRepairItemRequest out;
      ActorFoundationJsonVisitor archive(v);
      craft_codec::checked(archive.defer_duplicate_field_check_to_finish());
      repair_service_codec::Decode f{archive, v, "/deliver_repair_item"};
      repair_service_fields::fields(f, out);
      f.finish();
      return {true, std::move(out), {}};
    }
    if (kind == "receive_repair_item") {
      ReceiveRepairItemRequest out;
      ActorFoundationJsonVisitor archive(v);
      craft_codec::checked(archive.defer_duplicate_field_check_to_finish());
      repair_service_codec::Decode f{archive, v, "/receive_repair_item"};
      repair_service_fields::fields(f, out);
      f.finish();
      return {true, std::move(out), {}};
    }
    return {false,
            {},
            common_failure("UnsupportedRepairServiceCommand", kind,
                           ErrorKind::unsupported_capability)
                .error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  } catch (const exact_fields::CodecFailure &e) {
    return {false, {}, e.error};
  }
}
// 解码收据并核验原有形状；拒绝字段不全或来源不匹配的载荷。
Outcome<RepairServiceCommandReceipt>
decode_repair_service_command_receipt(const BoundaryValue &v) {
  try {
    RepairServiceCommandReceipt out;
    ActorFoundationJsonVisitor archive(v);
    craft_codec::checked(archive.defer_duplicate_field_check_to_finish());
    repair_service_codec::Decode f{archive, v, "/repair_service_command"};
    repair_service_fields::fields(f, out);
    f.finish();
    auto checked = validate_repair_service_command_receipt_shape(out);
    if (!checked.ok)
      return {false, {}, checked.error};
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  } catch (const exact_fields::CodecFailure &e) {
    return {false, {}, e.error};
  }
}
Status visit_repair_service_command_receipt(CodecVisitor&c,RepairServiceCommandReceipt&v){if(!c.reading()){auto checked=validate_repair_service_command_receipt_shape(v);if(!checked.ok)return checked;}repair_service_codec::Visit f{c};repair_service_fields::fields(f,v);auto status=f.result.ok?c.status():f.result;if(!status.ok)return status;return validate_repair_service_command_receipt_shape(v);}
Status emit_repair_service_command_receipt_fields(ProjectionWriter&w,const RepairServiceCommandReceipt&v){auto checked=validate_repair_service_command_receipt_shape(v);if(!checked.ok)return checked;repair_service_codec::Project f{w};repair_service_fields::fields(f,const_cast<RepairServiceCommandReceipt&>(v));return f.result.ok?w.status():f.result;}
Outcome<BoundaryValue> encode_repair_service_command_receipt(const RepairServiceCommandReceipt&v){BoundaryValue out;BoundaryProjectionWriter w(out);auto s=w.object("",[&](auto&fields){return emit_repair_service_command_receipt_fields(fields,v);});if(s.ok)s=w.finish();return s.ok?Outcome<BoundaryValue>{true,std::move(out),{}}:Outcome<BoundaryValue>{false,{},s.error};}
}
