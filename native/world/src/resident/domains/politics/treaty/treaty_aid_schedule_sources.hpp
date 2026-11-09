// 原 D1.1 援助调度身份：发生时刻只反读真实 activation，不分配伪业务 Fact。
#pragma once
#include "treaty_aid_occurrence.hpp"
#include "treaty_termination_sources.hpp"
namespace hanmo::resident {
struct TreatyAidScheduleSourceContext {
 const CommonStore& common;
 const OrderedTable<TreatyAidSchedule>& schedules;
 const TreatyTerminationContext& treaties;
};
// 只反读原条款与调度身份；财务成立前缀由原 occurrence/finance validator 核。
Outcome<const TreatyAidSchedule*> read_original_treaty_aid_schedule(
 const TreatyAidScheduleSourceContext&,const SourceRef&,Integer source_schema);
// 冷覆盖逐真实 activation 的每个 material_aid term，不能删行伪装无未履行责任。
Status validate_treaty_aid_schedule_sources(const TreatyAidScheduleSourceContext&);
// 仅在真实 activation 的同候选调用。该函数没有预留预算或成立债的副作用；
// 原调用栈随后必须完成 S5 接受/hold、当刻到期成立和完整 activation 后验才可发布。
Outcome<SourceRef> create_treaty_aid_schedule_in_candidate(CommonStore&,
 OrderedTable<TreatyAidSchedule>&,const TreatyAidScheduleSourceContext&,
 const SourceRef& actual_activation,const StableId& term_id);
}
