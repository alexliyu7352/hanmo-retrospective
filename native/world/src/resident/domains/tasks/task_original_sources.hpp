// Original Task receipt identity is read from the original row, then bound to its actual service application.
#pragma once
#include "task_result_time2.hpp"
namespace hanmo::resident {
Status validate_task_original_applied_source(const CommonStore&,const TaskState&,const SourceRef&,const TaskExactStartSource&,bool terminal,const WorldMoment&);
struct TaskOriginalAppliedTime {MomentOwnerSource applied_source;WorldMoment occurred_at;};
Outcome<TaskOriginalAppliedTime> read_task_original_applied_time(const CommonStore&,const TaskState&,const SourceRef&);
class TaskOriginalResultOwnerRead final:public ExactResultOwnerRead {
 const CommonStore&common_;const TaskState&tasks_;
public:
 TaskOriginalResultOwnerRead(const CommonStore&c,const TaskState&t):common_(c),tasks_(t){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
}
