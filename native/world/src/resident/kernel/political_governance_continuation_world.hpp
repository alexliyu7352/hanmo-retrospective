// 当前调用栈同时持原政治读、真实Task读与新续办包装器；不跨回滚、时钟或候选保存借用。
#pragma once
#include "resident/domains/politics/governance_continuation/political_governance_continuation_view.hpp"
#include "resident/domains/tasks/governance_task_rules.hpp"
#include <memory>
#include "political_governance_agency_world.hpp"
namespace hanmo::resident {
struct WorldBundle;
class PoliticalGovernanceContinuationWorldAccess final {
public:
 // takeover由同一上层调用自持，必须覆盖此对象及一次实际工作/清算调用的寿命。
 static Outcome<std::unique_ptr<PoliticalGovernanceContinuationWorldAccess>> create(const WorldBundle&,const PoliticalTakeoverRead& takeover,const DynamicStepInput* phase=nullptr);
 PoliticalGovernanceContinuationWorldAccess(const PoliticalGovernanceContinuationWorldAccess&)=delete;
 PoliticalGovernanceContinuationWorldAccess& operator=(const PoliticalGovernanceContinuationWorldAccess&)=delete;
 const PoliticalGovernanceContinuationContext& context()const{return context_;}
 const PoliticalGovernanceContinuationAccess& authority()const{return access_;}
private:
 PoliticalGovernanceContinuationWorldAccess(const WorldBundle&,const PoliticalTakeoverRead&,const DynamicStepInput*,std::unique_ptr<PoliticalGovernanceAgencyWorldAccess>);
 std::unique_ptr<PoliticalGovernanceAgencyWorldAccess> agencies_;
 PoliticsGovernanceAccess original_;GovernanceTaskRead tasks_;
 PoliticalGovernanceContinuationContext context_;PoliticalGovernanceContinuationAccess access_;
};
Status validate_political_governance_continuation_world(const WorldBundle&);
Outcome<PoliticalGovernanceContinuationQuote> prepare_political_governance_continuation_world(const WorldBundle&,const StableId& actor,const PoliticalContinueGovernance&);
Outcome<StableId> commit_political_governance_continuation_world(WorldBundle&,const StableId& actor,const PoliticalContinueGovernance&,const PoliticalGovernanceContinuationQuote&);
Outcome<BoundaryValue> query_political_governance_continuation_world(const WorldBundle&,const StableId& place,Integer cursor=0);
}
