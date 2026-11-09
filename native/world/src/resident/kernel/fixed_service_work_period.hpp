// 固定原 FWP /2 公共深值；持久源仍由原 Care owner 唯一持有。
#pragma once
#include "work_execution.hpp"
#include "fixed_service_history.hpp"
namespace hanmo::resident {
// /1 的整数五字段原样；/2 不再保存同一窗口的整数副本。
struct WorkPeriodRef2 {
 WorkExecutionBinding binding;
 Integer period_ordinal=0;
 WorldMoment from,until;
 Integer source_schema_version=2;
};
// 原 C21.1 字段序，工量为同 Fixed 信用的只读 WorkUnits 投影。
struct WorkPeriodView2 {
 WorkPeriodRef2 ref;
 std::vector<ActualActorWorkRole> actual_actor_roles;
 std::vector<StableId> actual_labor_allocations,facility_claim_refs;
 StableId work_unit_id;
 WorkUnits units_before,units_added,units_after;
 std::vector<SourceRef> resource_effect_refs,exertion_source_refs;
 WorkPeriodDisposition disposition=WorkPeriodDisposition::no_work;
 std::optional<SourceRef> reason;
};
Status validate_work_period_ref2(const WorkPeriodRef2&,ExactArithmeticBudget={});
Outcome<bool> same_work_period_ref2(const WorkPeriodRef2&,const WorkPeriodRef2&,ExactArithmeticBudget={});
Status validate_work_period_view2(const WorkPeriodView2&,ExactArithmeticBudget={});
// 只读原 FSL 的已供完整期；资格、worker、工位、资源和 Condition 仍须原 owner 反验。
// H 必须是原真实接受条款，函数另与同 Action required_units 严格配对。
struct FixedServiceWorkPeriodRead2 {
 WorkPeriodRef2 ref;
 WorkUnits units_before,units_added,units_after;
 ActualServiceIntervalView actual_service;
};
Outcome<FixedServiceWorkPeriodRead2> read_fixed_service_work_period2(
 const ActionElapsedStore&,const ActorActionWorkBinding&,Integer period_ordinal,
 Integer accepted_half_days,const WorldMoment& actual_now,const TimeScaleRef&,ExactArithmeticBudget={});
}
