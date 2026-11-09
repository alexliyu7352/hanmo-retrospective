#include "resident/model/regional_life_contract.hpp"
// 有限生活证据沿原服务/交易结果登记；所有读取与回收只借当前候选。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
class GrowthActionTimeRead;struct ActionTimeRegistration;
// 查询原action身份的真实结算；不存在时不推断已完成。
std::optional<LifeActivityView> growth_life_result(const GrowthState&,const StableId& action_id);
std::vector<StableId> growth_life_fact_ids(const GrowthState&,Integer after_tick,Integer through_tick);
std::optional<DynamicFactView> growth_life_dynamic_fact(const GrowthState&,const StableId& fact_id);
// 原资源recipe成功后调用；重复开工在任何资源变化之前由guard拒绝。
Status guard_life_activity_start(const CommonStore&,const GrowthState&,const GrowthCatalog&,const ActionCoreView&);
Status record_life_activity_start(CommonStore&,GrowthState&,const GrowthCatalog&,const ActionCoreView&,const GrowthQuote&,std::optional<Integer> actual_started_tick={});
Status record_life_activity_completion(const CommonStore&,GrowthState&,const GrowthCatalog&,const ActionCoreView&,Integer completed_tick);
// 取消仅登记终态；预留释放归原行动owner，已结算交易与服务预付款不回退。
Status cancel_life_activity(const CommonStore&,GrowthState&,const StableId& action_id,Integer tick,const StableId& code);
Status validate_life_activities(const CommonStore&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&);
Status validate_life_activities_at_moment(const CommonStore&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const GrowthActionTimeRead&,const std::vector<ActionTimeRegistration>&,const WorldMoment& actual_now);
// 先保留所有pin和活动来源，再保留每人最近16条未被引用终态；不改变资产。
Status prune_life_evidence(CommonStore&,GrowthState&,const std::vector<StableId>& pinned_ids);
// 新growth4专用冷字段；旧growth3不调用，不追加字节。
Status visit_life_activity_view(CodecVisitor&,LifeActivityView&);
}
