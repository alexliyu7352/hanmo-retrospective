// Life-v1c§4.1精确后继：原owner的一次真实完整N服务门，不是可写进度或另一个Action。
#pragma once
#include "work_execution.hpp"
namespace hanmo::resident {
struct LifeWorkServiceSourceRef {SourceRef source_ref;Integer source_schema=0;};
struct LifeWorkServiceSegment {
    MomentSpan actual_span;
    WorkUnits provided_service_units;
    std::vector<LifeWorkServiceSourceRef> actual_service_source_refs;
};
// 原业务永久来源body；暂停间隔不在segments中，累计仅ActionElapsed的WorkExecutionState。
struct LifeWorkPeriodView {
    LifeWorkPeriodSourceRef ref;
    WorkExecutionBinding execution_binding;
    LifeHalfDayStageGateProfile gate_profile=LifeHalfDayStageGateProfile::medicine_batch;
    SourceRef stage_rule_source_ref;
    Integer completed_halfday_ordinal=0;
    WorldMoment applied_at;
    std::vector<LifeWorkServiceSegment> service_segments;
    WorkUnits consumed_service_units;
    std::vector<ActualActorWorkRole> actual_actor_roles;
    std::vector<StableId> actual_labor_allocations,facility_claim_refs;
    std::vector<SourceRef> resource_effect_refs,exertion_source_refs,original_stage_result_refs;
    std::vector<Integer> completed_stage_ordinals;
};
class LifeWorkPeriodSourceRead {
public:
    virtual ~LifeWorkPeriodSourceRead()=default;
    virtual Outcome<LifeWorkPeriodView> read_life_work_period(const LifeWorkPeriodSourceRef&)const=0;
    // 原owner须独立反验实际人员/工位/投入/用力/工序及每段真实source，不能拿S0成功互证。
    virtual Status validate_original_life_work_period(const LifeWorkPeriodView&)const=0;
};
bool same_life_work_period_ref(const LifeWorkPeriodSourceRef&,const LifeWorkPeriodSourceRef&);
Status validate_life_work_period_value(const LifeWorkPeriodView&,Integer units_per_half_day,ExactArithmeticBudget={});
Outcome<LifeWorkPeriodView> read_verified_life_work_period(const LifeWorkPeriodSourceRef&,const LifeWorkPeriodSourceRead&,Integer units_per_half_day,ExactArithmeticBudget={});
}
