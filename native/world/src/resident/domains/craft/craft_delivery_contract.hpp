// S9 owns inspection/binding; Tasks/Common remain the only goods and money writers.
#pragma once
#include "resident/model/objective_evidence.hpp"
namespace hanmo::resident {
struct CraftDeliveryLine {std::optional<StableId> instance_id;StableId inventory_id,lot_id,item_id;Integer quantity=0;StableId unit_id;SourceRef actual_transfer_result_ref;};
enum class CraftDeliveryResult {accepted_partial,accepted_final};
enum class CraftDeliverySettlementState {open,settling,closed};
struct CraftDeliveryReceiptView {StableId receipt_id,job_id;SourceRef order_ref,inspection_result_ref,task_delivery_result_ref;std::vector<CraftDeliveryLine> lines;Owner from_owner,to_owner;StableId acceptor_actor_id,acceptance_kind;Integer acceptance_version=0;CraftDeliveryResult result=CraftDeliveryResult::accepted_partial;Integer completed_tick=0,remaining_quantity=0;bool rework_required=false;CraftDeliverySettlementState settlement_state=CraftDeliverySettlementState::open;WorldMoment completed_at;StableId source_rule_version;};
class CraftDeliveryReceiptRead {public:virtual ~CraftDeliveryReceiptRead()=default;virtual Outcome<std::optional<CraftDeliveryReceiptView>> read_craft_delivery_receipt(const ObjectiveReceiptRef&)const=0;virtual Outcome<bool> read_actual_craft_participant(const ObjectiveReceiptRef&,const StableId&actor_id)const=0;};
}
