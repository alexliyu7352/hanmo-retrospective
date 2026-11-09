// Original quantity-contract Calendar close. The row belongs to that Contract;
// it is neither a second deadline table nor an ActionElapsed business payload.
#pragma once
#include "resident/domains/affairs/state_need.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
inline constexpr const char* task_quantity_deadline_closed_kind="task_quantity_deadline_closed";
struct TaskQuantityDeadlineClosure {
 StableId source_id,task_id;
 MomentOwnerSource original_acceptance_start_ref;
 StateNeedDeadline frozen_deadline_gate;
 MomentOwnerSource original_period_opening;
 WorldMoment closed_at;
};
struct TaskExactLifeSources;struct TaskState;struct ContractRecord;struct TaskRecord;struct SocialPeriodSources;
class CommonStore;class CurrentMomentRead;class ProjectionWriter;struct CodecVisitor;
struct TaskQuantityDeadlineContext {
 const CommonStore& common;const TaskState& tasks;
 const SocialPeriodSources& periods;const CatalogIdentity& identity;
 const CurrentMomentRead& clock;const TaskExactLifeSources* life=nullptr;
};
// This writes only at the existing original Calendar expiry branch. It returns
// the retained same row on a true retry. Pending Actions require their original
// disposition policy; this source never invents an interruption or completion.
Outcome<SourceRef> record_task_quantity_deadline_close_in_candidate(CommonStore&,TaskState&,
 const TaskQuantityDeadlineContext&,const StableId& contract_id,const ExactPeriodContext&);
Outcome<const TaskQuantityDeadlineClosure*> read_task_quantity_deadline_closure(
 const TaskQuantityDeadlineContext&,const SourceRef&,Integer source_schema);
Status validate_task_quantity_deadline_sources(const TaskQuantityDeadlineContext&);
// Source-local P10 leg deliberately does not call settlement, full Task, Social
// knowledge or any later Common command result.
Outcome<WorldMoment> read_task_quantity_deadline_source_moment(const CommonStore&,const TaskState&,const SourceRef&,Integer,const TaskExactLifeSources* life=nullptr);
// The completed source supplies its own actual M. No caller-supplied timestamp.
Outcome<bool> read_task_quantity_result_deadline_eligibility(const CommonStore&,const TaskState&,const StableId& task_id,const SourceRef& actual_result,const TaskExactLifeSources* life=nullptr);
class TaskQuantityDeadlinePinSink {public:virtual ~TaskQuantityDeadlinePinSink()=default;virtual Status pin(const MomentOwnerSource&)=0;};
Status visit_task_quantity_deadline_source_pins(const TaskQuantityDeadlineContext&,TaskQuantityDeadlinePinSink&);
Outcome<TaskQuantityDeadlineClosure> decode_task_quantity_deadline_closure(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_quantity_deadline_closure(const TaskQuantityDeadlineClosure&);
Status visit_task_quantity_deadline_closure(CodecVisitor&,TaskQuantityDeadlineClosure&);
Status emit_task_quantity_deadline_closure_fields(ProjectionWriter&,const TaskQuantityDeadlineClosure&);
// Quantity payload only. Full dynamic_tasks3/4 roots remain independently
// required; these functions cannot publish a shortened whole dynamic section.
Outcome<ContractRecord> decode_quantity_contract_record(const BoundaryValue&,Integer physical_version);
Outcome<BoundaryValue> encode_quantity_contract_record(const ContractRecord&,Integer physical_version);
Status visit_quantity_contract_record(CodecVisitor&,ContractRecord&,Integer physical_version);
Status emit_quantity_contract_record_fields(ProjectionWriter&,const ContractRecord&,Integer physical_version);
Outcome<bool> emit_task_quantity_deadline_fact_payload(ProjectionWriter&,const TaskState&,const FactRecord&);
}
