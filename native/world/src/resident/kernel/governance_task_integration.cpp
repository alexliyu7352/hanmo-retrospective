// 三域写入都属于调用方持有的同一WorldBundle候选；此处不创建嵌套保存点或另一个循环时钟。
#include "political_takeover_execution_world.hpp"
#include "governance_task_integration.hpp"
#include "political_governance_continuation_world.hpp"
#include "world_bundle.hpp"
#include "resident/domains/politics/politics_governance.hpp"
#include "resident/domains/politics/politics_governance_consequences.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// gate来自两个显式作者域的共同校验；缺任何所需目录属于坏候选，不能静默跳过已接责任。
Status available(const WorldBundle& w){
    if(!w.governance_tasks_enabled())return common_failure("GovernanceNotEnabled",{},ErrorKind::unsupported_capability);
    if(!w.catalog->politics_governance||!w.catalog->politics_governance->enabled||!w.catalog->public_relief||!w.catalog->organization)return common_failure("MissingGovernancePhaseCatalog",{},ErrorKind::internal);
    return common_success();
}
// 单半日边界在加一前先核溢出；前后两入口分别核共同clock的实际位置。
bool single_boundary(const DynamicStepInput& phase){return phase.from_tick>=0&&phase.from_tick<std::numeric_limits<Integer>::max()&&phase.to_tick==phase.from_tick+1;}
}
Outcome<GovernanceTaskStep> step_governance_task_world(WorldBundle& w,const StableId& action_id,const DynamicStepInput& phase){
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(w,&phase);if(!takeover.ok)return {false,{},takeover.error};
    auto enabled=available(w);if(!enabled.ok)return {false,{},enabled.error};
    if(!single_boundary(phase)||w.common.tick()!=phase.from_tick)return {false,{},common_failure("GovernancePhaseBoundaryMismatch",action_id,ErrorKind::internal).error};
    auto agencies=PoliticalGovernanceAgencyWorldAccess::create(w,&phase);if(!agencies.ok)return {false,{},agencies.error};
    PoliticsGovernanceAccess authority(w.common,w.politics,w.politics_governance,*w.catalog->politics_governance,w.organization,*w.catalog->organization,w.services,agencies.value?&agencies.value->read():nullptr);
    const GovernanceOutcomeFreeze freeze=[&](const TaskGovernanceRead& tasks,const GovernanceWorkCompletion& work){
        return record_governance_outcome_plan_in_transaction(w.common,w.politics_governance,*w.catalog->politics_governance,tasks,work.fact_id,phase);
    };
    const GovernanceReleaseResources release=[&](const TaskGovernanceRead& tasks,const GovernanceTaskSource& source){
        return release_public_relief_in_candidate(w.common,w.public_relief,tasks,source.acceptance.task_id,&phase);
    };
    std::unique_ptr<PoliticalGovernanceContinuationWorldAccess> continuation;const GovernanceAuthorityRead* effective=&authority;
    if(w.catalog->political_governance_continuations){if(!takeover.value->responsibilities())return {false,{},common_failure("GovernanceContinuationTakeoverUnavailable").error};auto read=PoliticalGovernanceContinuationWorldAccess::create(w,*takeover.value->responsibilities(),&phase);if(!read.ok)return {false,{},read.error};continuation=std::move(read.value);effective=&continuation->authority();}
    return step_governance_task_action(w.common,w.tasks,w.governance_tasks,*effective,action_id,phase.to_tick,freeze,release,takeover.value->tasks());
}
Status settle_governance_task_world(WorldBundle& w,const DynamicStepInput& phase){
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(w);if(!takeover.ok)return {false,{},takeover.error};std::optional<EconomicTakeoverPhaseContext> execution;if(takeover.value->economy())execution.emplace(EconomicTakeoverPhaseContext{*takeover.value->economy(),w.economic_takeover_execution});
    if(!w.governance_tasks_enabled())return common_success();
    auto enabled=available(w);if(!enabled.ok)return enabled;
    if(!single_boundary(phase)||w.common.tick()!=phase.to_tick)return common_failure("GovernanceSettlementBeforeClock",{},ErrorKind::internal);
    auto agencies=PoliticalGovernanceAgencyWorldAccess::create(w);if(!agencies.ok)return {false,{},agencies.error};
    PoliticsGovernanceAccess authority(w.common,w.politics,w.politics_governance,*w.catalog->politics_governance,w.organization,*w.catalog->organization,w.services,agencies.value?&agencies.value->read():nullptr);
    PublicReliefRead receipts(w.common,w.public_relief);
    std::unique_ptr<PoliticalGovernanceContinuationWorldAccess> continuation;const GovernanceAuthorityRead* effective=&authority;
    if(w.catalog->political_governance_continuations){if(!takeover.value->responsibilities())return common_failure("GovernanceContinuationTakeoverUnavailable");auto read=PoliticalGovernanceContinuationWorldAccess::create(w,*takeover.value->responsibilities());if(!read.ok)return {false,{},read.error};continuation=std::move(read.value);effective=&continuation->authority();}
    GovernanceSettlementServices services;
    services.distribute=[&](const TaskGovernanceRead& tasks,const GovernanceTaskSource& source){
        return settle_public_relief_in_candidate(w.common,w.local_economy,w.public_relief,tasks,*effective,source.acceptance.task_id,execution?&*execution:nullptr);
    };
    services.receipts=&receipts;
    services.apply_outcome=[&](const TaskGovernanceRead& tasks,const GovernanceTaskCompletion& final)->Status{
        auto result=apply_governance_civil_consequence_in_transaction(w.common,w.politics,w.politics_governance,*w.catalog->politics_governance,tasks,receipts,final.fact_id);
        if(!result.ok)return {false,{},result.error};
        if(!result.value)return common_failure("MissingGovernanceCivilConsequence",final.fact_id,ErrorKind::internal);
        return common_success();
    };
    return settle_ready_governance_tasks(w.common,w.tasks,w.governance_tasks,services);
}
}
