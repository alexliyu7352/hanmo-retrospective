// IC-V1a 8.1/8.2.1. S3 owns this one shared contract; original owners produce outcomes.
#pragma once
#include "work_execution_contract.hpp"
#include "resident/domains/interaction/communication_delivery_read.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
enum class WorkApplicationKind {patrol_security,production_improvement,facility_activation};
struct OutcomeApplication {
 StableId task_id,objective_id;SourceRef program_ref;StableId target_line_id;WorkResultSource work_result_ref;
 WorkApplicationKind application_kind=WorkApplicationKind::patrol_security;SourceRef original_application_ref;
};
struct ReportBack {
 StableId task_id,report_requirement_id;WorkResultSource work_result_ref;SourceRef report_content_ref,original_reporting_obligation_ref;
 RegisteredAddress addressee;
};
using AcceptedWorkFollowThroughRef=std::variant<OutcomeApplication,ReportBack>;
struct PatrolApplicationUnavailable {SourceRef program_ref;StableId target_line_id;SourceRef application_terminal_ref;};
struct ImprovementApplicationUnavailable {WorkRef work_ref;SourceRef application_terminal_ref;};
struct FacilityApplicationUnavailable {SourceRef asset_ref,application_terminal_ref;};
struct ReportBackUnavailable {SourceRef reporting_obligation_ref,report_terminal_ref;};
using WorkFollowThroughUnavailableSource=std::variant<PatrolApplicationUnavailable,ImprovementApplicationUnavailable,FacilityApplicationUnavailable,ReportBackUnavailable>;
struct FollowThroughTerminalView {
 SourceRef original_obligation_ref,accepted_goal_ref;WorkResultSource work_result_ref;
 StableId source_terminal_id;Integer source_schema=0;SourceRef actual_cause_ref,continuity_source_ref;
 std::vector<SourceRef> preserved_actual_result_refs;WorldMoment effective_game_time;
};
struct WorkFollowThroughFulfilled {std::vector<SourceRef> actual_result_refs;};
struct WorkFollowThroughTemporarilyBlocked {SourceRef current_reason_source_ref,continuity_source_ref;};
struct WorkFollowThroughPermanentlyUnfulfillable {WorkFollowThroughUnavailableSource source;FollowThroughTerminalView terminal;};
using WorkFollowThroughReadResult=std::variant<WorkFollowThroughFulfilled,WorkFollowThroughTemporarilyBlocked,WorkFollowThroughPermanentlyUnfulfillable>;
// This input is a frozen original accepted goal and terms, never a new requested goal.
struct WorkFollowThroughQuery {AcceptedWorkFollowThroughRef follow_through_ref;SourceRef accepted_goal_ref;TermsRevisionRef accepted_terms_ref;};
class WorkFollowThroughRead {
public:virtual ~WorkFollowThroughRead()=default;
 virtual Outcome<WorkFollowThroughReadResult> read_work_follow_through(const WorkFollowThroughQuery&,const ReadStamp&)const=0;
 virtual Outcome<FollowThroughTerminalView> read_work_follow_through_terminal(const WorkFollowThroughQuery&,const WorkFollowThroughUnavailableSource&,const ReadStamp&)const=0;
};
struct S3WorkCommission {StableId work_id;SourceRef accepted_task_ref,work_acceptance_source_ref,expense_terms_ref;};
struct WorkCommissionTerminal {StableId task_id;SourceRef accepted_task_ref;StableId work_id,task_terminal_fact_id;Integer source_schema=1;};
struct WorkExecutionStopped {WorkResultSource stopped_work_result_ref;};
struct AcceptedFollowThroughStopped {
 WorkResultSource completed_work_result_ref;std::vector<AcceptedWorkFollowThroughRef> failed_follow_through_refs;
 std::vector<WorkFollowThroughUnavailableSource> source_terminal_refs;std::vector<SourceRef> fulfilled_goal_refs;
};
using WorkCommissionTerminationBasis=std::variant<WorkExecutionStopped,AcceptedFollowThroughStopped>;
// One Task terminal. Work completed remains immutable if a later accepted duty stops.
struct WorkCommissionTaskTerminalSource {
 StableId task_terminal_fact_id;S3WorkCommission accepted_scope_ref;TermsRevisionRef accepted_terms_ref;
 WorkResultSource work_result_ref;WorkCommissionTerminationBasis termination_basis;
 std::vector<SourceRef> preserved_fulfilment_refs,remaining_financial_or_return_refs;WorldMoment effective_game_time;
};
bool same_work_follow_through(const AcceptedWorkFollowThroughRef&,const AcceptedWorkFollowThroughRef&);
Status validate_work_follow_through_query(const WorkFollowThroughQuery&);
Status validate_work_follow_through_result(const WorkFollowThroughQuery&,const WorkFollowThroughReadResult&);
const SourceRef& work_follow_through_terminal_source(const WorkFollowThroughUnavailableSource&);
Status visit_work_follow_through_ref(CodecVisitor&,AcceptedWorkFollowThroughRef&);
Status visit_work_follow_through_terminal(CodecVisitor&,FollowThroughTerminalView&);
Status visit_work_commission_terminal(CodecVisitor&,WorkCommissionTaskTerminalSource&);
}
