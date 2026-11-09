// 接替接受是原 Work 执行职责的本人同意，不另建 Task、费用或付款权。
#pragma once
#include "work_task_sources.hpp"
#include "resident/domains/politics/governance_targets/governance_work_lifecycle_sources.hpp"
namespace hanmo::resident {
struct WorkReassignmentSourceContext {
 const WorkTaskSourceContext& tasks;
 const GovernanceWorkLifecycleSourceContext& governance;
 const ReadStamp& original_sources_stamp;
};
Status validate_work_reassignment_acceptance_request(const WorkReassignmentSourceContext&,const SourceRef& approval,const StableId& authenticated_actor);
Outcome<SourceRef> accept_work_reassignment_in_candidate(CommonStore&,TaskState&,const WorkReassignmentSourceContext&,const SourceRef& approval,const StableId& authenticated_actor);
Outcome<const WorkReassignmentAcceptanceSource*> read_work_reassignment_acceptance(const WorkReassignmentSourceContext&,const SourceRef&);
Status validate_work_reassignment_acceptances(const WorkReassignmentSourceContext&);
}
