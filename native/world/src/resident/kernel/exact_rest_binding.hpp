// 原Growth rest真实源与唯一S0经过量的同候选适配，没有公开来源写入口。
#pragma once
#include "resident/domains/actor_foundation/rest_condition.hpp"
#include "resident/domains/growth/growth_fixed_service_model.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CommitResult;struct ActionCompletion;
// 从本Action实际Completed源读Moment，供Condition冷读，不从start+H倒造。
class ExactRestTimeRead final:public AuthoredRestTimeRead {
 const struct GrowthState& growth_;
public:
 explicit ExactRestTimeRead(const GrowthState& growth):growth_(growth){}
 Outcome<WorldMoment> completed_rest_at(const StableId&)const override;
};
// 原开工及资源支付成功、原真实结果ID分配后才建立Started和唯一elapsed。
Status bind_exact_rest_started(WorldBundle&,const StableId& actor,Integer request_seq,const CommitResult&);
// 真实到期仍调用原Growth完成handler；Condition只绑定其一次实际写，不另跑身体公式。
Outcome<ActionCompletion> finish_exact_rest_in_candidate(WorldBundle&,const ActionCoreView&,const WorldMoment&,std::vector<BoundAuthoredRestConditionWrite>&);
// 冷读全部原源/Action/起止/ref/真实条款，缺任一来源均拒绝。
Status validate_exact_rest_sources(const WorldBundle&);
}
