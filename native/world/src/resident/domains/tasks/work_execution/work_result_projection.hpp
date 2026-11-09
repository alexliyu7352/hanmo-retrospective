#pragma once
#include "work_task_sources.hpp"
#include "work_supervision_sources.hpp"
#include "work_commission_terminal.hpp"
#include "resident/kernel/dynamic_result_time2.hpp"
namespace hanmo::resident {
// A call-scoped lease must retain these original readers and this validated stamp.
// No world copy, timestamp registry or synthetic report is held here.
struct WorkResultProjectionAccess {
 const CommonStore& common;const TaskState& tasks;const AffairsState& affairs;
 const WorkOriginalRead& original;const WorkTaskRead& definitions;
 const WorkCommissionTerminalContext* terminals=nullptr;const ReadStamp* terminal_stamp=nullptr;
 const SupervisionSourceContext* supervision=nullptr;
};
bool native_work_result_kind(const FactRecord&);
class NativeWorkResultOwnerRead final:public ExactResultOwnerRead {
 WorkResultProjectionAccess access_;
public:explicit NativeWorkResultOwnerRead(WorkResultProjectionAccess x):access_(x){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
}
