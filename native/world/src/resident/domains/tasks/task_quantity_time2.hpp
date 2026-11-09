// T3.4 original exact quantity Task/Action/result identity. No second task or
// completion ledger; every result is the Common result sealed by its owner.
#pragma once
#include "task_model.hpp"
namespace hanmo::resident {
struct CommitResult;
class ProjectionWriter;
struct ExactQuantityActionCompletion {
 const TaskRecord* task=nullptr;const ContractRecord* contract=nullptr;
 const TaskExactStartSource* start=nullptr;const TaskExactTerminalSource* terminal=nullptr;
 const CommitResult* committed_result=nullptr;
};
// Pure source-local legs do not call quantity settlement, fees or the P10
// collector. Complete ActionElapsed and quantity validators remain independent.
Outcome<ExactQuantityActionCompletion> read_exact_quantity_action_completion(
 const CommonStore&,const TaskState&,const StableId& actual_action_id,const SourceRef& actual_committed_result);
// Capture-time source leg: actual original completion and payload/header only.
// It does not require the later Common seal or call Knowledge/whole Task cold.
Outcome<ExactQuantityActionCompletion> read_exact_quantity_applied_source(
 const CommonStore&,const TaskState&,const SourceRef&,Integer source_schema);
Status bind_exact_quantity_action_result_in_candidate(const CommonStore&,TaskState&,
 const StableId& actual_action_id,const SourceRef& actual_committed_result);
Status validate_exact_quantity_active_action(const CommonStore&,const TaskState&,const ActionCoreView&);
Status bind_exact_quantity_advance_result_in_candidate(const CommonStore&,TaskState&,const SourceRef& actual_sealed_advance,const TaskExactLifeSources* life=nullptr);
Status validate_exact_quantity_result_bindings(const CommonStore&,const TaskState&,const TaskExactLifeSources* life=nullptr);
// Current record row fields only; the complete tasks2/3 root still owns its
// other required roots. This does not advertise an empty Objective/life owner.
Outcome<TaskRecord> decode_exact_quantity_task_record(const BoundaryValue&);
Outcome<BoundaryValue> encode_exact_quantity_task_record(const TaskRecord&);
Status visit_exact_quantity_task_record(CodecVisitor&,TaskRecord&);
Status emit_exact_quantity_task_record_fields(ProjectionWriter&,const TaskRecord&);
Outcome<DeliveryFact> decode_task_quantity_delivery_source2(const BoundaryValue&);
Outcome<TaskTransferFact> decode_task_quantity_transfer_source2(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_quantity_delivery_source2(const DeliveryFact&);
Outcome<BoundaryValue> encode_task_quantity_transfer_source2(const TaskTransferFact&);
Status visit_task_quantity_delivery_source2(CodecVisitor&,DeliveryFact&);
Status visit_task_quantity_transfer_source2(CodecVisitor&,TaskTransferFact&);
Status emit_task_quantity_delivery_source2_fields(ProjectionWriter&,const DeliveryFact&);
Status emit_task_quantity_transfer_source2_fields(ProjectionWriter&,const TaskTransferFact&);
}
