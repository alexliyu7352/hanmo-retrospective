#pragma once
#include "director_subject2.hpp"
#include "resident/domains/tasks/task_result_time2.hpp"
namespace hanmo::resident {
// Only actual original Task acceptance/execution/settlement sources disclose this wake to their parties.
Status visit_task_commitment_wakes2(const CommonStore&,const TaskState&,const AffairsState&,const WorldMoment&,const std::function<Status(const SubjectWake2&)>&);
Status validate_director_task_commitment_wake2(const CommonStore&,const TaskState&,const AffairsState&,const SubjectWake2&,const WorldMoment&);
Status collect_director_task_commitment_wakes2(const CommonStore&,const TaskState&,const AffairsState&,DirectorState&,const WorldMoment&);
}
