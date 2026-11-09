#pragma once
#include "work_execution_contract.hpp"
#include "resident/domains/military/duty/military_duty_contract.hpp"
namespace hanmo::resident {
// The original accepted Task/order binds a duty to this scope. Matching actions
// or leaders alone never transfers another commission's service to this Work.
class WorkMilitaryAssignmentRead {
public:virtual ~WorkMilitaryAssignmentRead()=default;
 virtual Status validate_work_duty_assignment(const WorkAuthorizationRef&,const MilitaryDutyTerms&,const MilitaryDutyAcceptedSource&)const=0;
 virtual Outcome<WorkMilitaryActivityBinding> read_registered_duty_activity(const DutyRegisteredMilitaryActivity&)const=0;
};
Outcome<WorkPeriodRead> read_work_military_period(const CommonStore&,const TaskWorkExecutionState&,const MilitaryDutyState&,const MilitaryDutyContext&,const WorkMilitaryAssignmentRead&,const WorkEpisode&,const WorkMilitaryPeriodSource&);
}
