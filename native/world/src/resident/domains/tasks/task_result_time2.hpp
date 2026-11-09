#pragma once
#include "task_model.hpp"
#include "resident/kernel/dynamic_result_time2.hpp"
namespace hanmo::resident {
struct AffairsState;struct TaskExactLifeSources;
Outcome<ExactOwnerFactRead> read_task_dynamic_fact_v2(const CommonStore&,const TaskState&,const AffairsState&,const FactRecord&,const WorkOriginalRead* work_sources=nullptr,const TaskExactLifeSources* life=nullptr);
Outcome<WorldMoment> read_task_result_source_moment(const CommonStore&,const TaskState&,const AffairsState&,const MomentOwnerSource&,const WorkOriginalRead* work_sources=nullptr,const TaskExactLifeSources* life=nullptr);
}
