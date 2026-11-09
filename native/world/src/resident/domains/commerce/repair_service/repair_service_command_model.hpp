// Typed original command values. Request values contain no future source IDs.
#pragma once
#include "repair_service.hpp"
#include <string_view>
namespace hanmo::resident {
class CodecVisitor;class ProjectionWriter;
inline constexpr bool repair_service_command_kind(std::string_view kind)noexcept{return kind=="offer_repair_service"||kind=="accept_repair_service_offer"||kind=="deliver_repair_item"||kind=="receive_repair_item";}
struct OfferRepairServiceRequest {RepairServiceOfferValues terms;RepairServiceAcceptanceInspection inspection;MomentOwnerSource actual_contact_source;};
struct AcceptRepairServiceOfferRequest {MomentOwnerSource offer_ref,actual_contact_source;};
struct DeliverRepairItemRequest {MomentOwnerSource contract_ref;CargoSelection actual_item_selection;MomentOwnerSource actual_contact_source;};
struct ReceiveRepairItemRequest {MomentOwnerSource contract_ref;StableId actual_destination_inventory;MomentOwnerSource actual_contact_source;};
using RepairServiceCommandRequest=std::variant<OfferRepairServiceRequest,AcceptRepairServiceOfferRequest,DeliverRepairItemRequest,ReceiveRepairItemRequest>;
struct RepairServiceCommandReceipt {
 StableId command_kind,actor_id;MomentOwnerSource actual_source;WorldMoment occurred_at;
 std::optional<RepairServiceAcceptedResult> accepted_result;
};
struct ReceiptDetails;
// Central implements only this exact ReceiptDetails variant extraction. The
// domain then verifies the actual original cached result/history and payload.
const RepairServiceCommandReceipt* repair_service_command_receipt(const ReceiptDetails&)noexcept;
Status validate_repair_service_command_receipt_shape(const RepairServiceCommandReceipt&);
Outcome<RepairServiceCommandRequest> decode_repair_service_command_request(const StableId& command_kind,const BoundaryValue&);
Outcome<RepairServiceCommandReceipt> decode_repair_service_command_receipt(const BoundaryValue&);
Outcome<BoundaryValue> encode_repair_service_command_receipt(const RepairServiceCommandReceipt&);
Status visit_repair_service_command_receipt(CodecVisitor&,RepairServiceCommandReceipt&);
Status emit_repair_service_command_receipt_fields(ProjectionWriter&,const RepairServiceCommandReceipt&);
}
