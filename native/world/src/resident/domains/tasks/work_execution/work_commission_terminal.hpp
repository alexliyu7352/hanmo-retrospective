// IC-V1a 8.2.1: the original Task has one terminal after real service/goal closure.
#pragma once
#include "work_follow_through.hpp"
namespace hanmo::resident {
struct WorkCommissionRequiredAlternatives {std::vector<WorkFollowThroughQuery> alternatives;};
struct WorkCommissionAcceptedResponsibilities {
 StableId task_id;S3WorkCommission scope;TermsRevisionRef accepted_terms;
 // Each group is required; one genuinely fulfilled alternative fulfils a group.
 std::vector<WorkCommissionRequiredAlternatives> required_groups;
};
class WorkCommissionResponsibilitiesRead {
public:virtual ~WorkCommissionResponsibilitiesRead()=default;
 virtual Status validate_work_commission_stamp(const ReadStamp&)const=0;
 virtual Outcome<WorkCommissionAcceptedResponsibilities> read_accepted_work_responsibilities(const S3WorkCommission&,const WorkResultSource&,const ReadStamp&)const=0;
 // Re-read original owner stop/continuity sources and all remaining goals. Any
 // still-running service must be safely closed first; money/return duties stay real.
 virtual Outcome<std::vector<SourceRef>> read_safe_work_commission_remainders(const S3WorkCommission&,const WorkResultSource&,const std::vector<WorkFollowThroughUnavailableSource>&,const ReadStamp&)const=0;
 virtual Status validate_historical_work_commission_terminal(const WorkCommissionTaskTerminalSource&,const ReadStamp&)const=0;
};
struct TaskServiceTerminalState {Integer version=1;OrderedTable<WorkCommissionTaskTerminalSource> work_commission_terminals;};
struct WorkCommissionTerminalContext {
 const CommonStore& common;const TaskState& tasks;const WorkExecutionRead& work;
 const WorkCommissionResponsibilitiesRead& responsibilities;const WorkFollowThroughRead& follow_through;
 const CurrentMomentRead& clock;
};
// Null means the actual accepted service is still fulfilable (including temporary
// blocking), or normally fulfilled; no synthetic terminal is made in either case.
Outcome<std::optional<WorkCommissionTerminal>> close_unfulfillable_work_commission_in_candidate(CommonStore&,TaskState&,const WorkCommissionTerminalContext&,const S3WorkCommission&,const ReadStamp&);
Outcome<const WorkCommissionTaskTerminalSource*> read_work_commission_termination(const WorkCommissionTerminalContext&,const S3WorkCommission&,const WorkCommissionTerminal&,const ReadStamp&);
// Same actual Task/Work terminal and accepted responsibilities; the Work result
// uses its original financial evidence leg to avoid re-entering these S5 debts.
Outcome<const WorkCommissionTaskTerminalSource*> read_work_commission_termination_for_financial_evidence(const WorkCommissionTerminalContext&,const S3WorkCommission&,const WorkCommissionTerminal&,const ReadStamp&);
Status validate_task_service_terminals(const WorkCommissionTerminalContext&,const ReadStamp&);
Status visit_task_service_terminals(CodecVisitor&,TaskServiceTerminalState&);
Outcome<TaskServiceTerminalState> decode_task_service_terminals(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_service_terminals(const TaskServiceTerminalState&);
Status emit_task_service_terminals_projection(ProjectionWriter&,std::string_view,const TaskServiceTerminalState&);
}
