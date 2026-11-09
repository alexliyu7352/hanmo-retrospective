// 续办只解除原已受领安民责任的改派暂停，不重开Task、不换人、不改预算或剩余劳动。
#pragma once
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/politics/politics_governance.hpp"
#include "resident/domains/politics/takeover/politics_takeover_contract.hpp"
#include "resident/domains/tasks/governance_task_contract.hpp"
#include "resident/domains/family/family_death.hpp"
namespace hanmo::resident {
inline constexpr const char* political_governance_continuation_module="political_governance_continuations";
inline constexpr const char* political_governance_continue_command="political_continue_governance";
inline constexpr const char* political_governance_continue_kind="political_governance_continued";
inline constexpr const char* political_governance_continue_prefix="political_governance_continuation";
inline constexpr std::uint64_t political_governance_continuation_feature=16777216u;
// 玩家只能选原受领任务与其当前完整接管决定；所有人、钱库和条款均从三域真实来源取得。
struct PoliticalContinueGovernance {
 ActingAuthority authority;
 StableId task_id,registration_fact_id,approval_fact_id,decision_fact_id;
};
// 原Task与合同永久引用保留；新署只承担这一同ID责任，原签发人和受益人不改派。
struct PoliticalGovernanceContinuation {
 StableId id,task_id,acceptance_fact_id,authority_fact_id,contract_id,executor_actor_id;
 StableId jurisdiction_id,site_id,original_organization_id,public_wallet_id,public_inventory_id;
 StableId registration_fact_id,approval_fact_id,decision_fact_id,occupation_fact_id,grant_id;
 ActingAuthority authority;
 Integer tick=0;
};
// 真实玩家提交尾才填绑定；本域临时writer不能臆造Common结果或以相邻ID猜结果。
struct GovernanceContinuationPlayerReceipt {
 StableId source_fact_id,actor_id,result_id;Integer tick=0,request_sequence=0,result_revision=0;
};
struct PoliticalGovernanceContinuationState {
 Integer version=1;
 OrderedTable<PoliticalGovernanceContinuation> continuations;
 OrderedTable<GovernanceContinuationPlayerReceipt> player_receipts;
};
// original_authority必须是原PoliticsGovernanceAccess，不能回借下面的续办包装器。
// Task与政治接管只读都同步借同候选；完整Task/原治理/军事史仍由World在外层先验。
struct PoliticalGovernanceContinuationContext {
 const CommonStore& common;const PoliticsState& politics;
 const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;
 const GovernanceAuthorityRead& original_authority;const TaskGovernanceRead& tasks;const PoliticalTakeoverRead& takeover;
 const PoliticalGovernanceContinuationState& state;
 FamilyDeathOriginRead death_origin;
 const DynamicStepInput* phase=nullptr;
};
struct PoliticalGovernanceContinuationQuote {WorldIdentity identity;PoliticalContinueGovernance command;PoliticalGovernanceContinuation source;};
Status validate_political_governance_continuation_modules(const std::vector<std::pair<StableId,StableId>>&,bool enabled);
Status validate_political_governance_continuation_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>& modules,bool enabled,const PoliticalGovernanceContinuationState*);
Outcome<bool> decode_political_governance_continuation_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_political_governance_continuation_fact_header(const CommonStore&,const PoliticalGovernanceContinuationState&,const FactRecord&);
// 业务事实先核完整原来源；永久玩家结果另在真实Common提交完成后校验。
Status validate_political_governance_continuations(const PoliticalGovernanceContinuationContext&);
struct GovernanceTaskState;struct PublicReliefState;
// 工作完成/付款各按真实原事实之前的政治决定与续办来源核验，不增加S3/S5历史字段。
Status validate_political_governance_continued_execution(const PoliticalGovernanceContinuationContext&,const GovernanceTaskState&,const PublicReliefState&);
Outcome<PoliticalGovernanceContinuationQuote> preview_political_governance_continuation(const PoliticalGovernanceContinuationContext&,const StableId& actor,const PoliticalContinueGovernance&);
Outcome<StableId> apply_political_governance_continuation_in_candidate(CommonStore&,PoliticalGovernanceContinuationState&,const PoliticalGovernanceContinuationContext&,const StableId& actor,const PoliticalContinueGovernance&,const PoliticalGovernanceContinuationQuote&);
// 包装器只替换具名ControlOrAssetsChanged/OfficeVacant暂停；死亡、解散、健康暂停和未知失败原样返回。
class PoliticalGovernanceContinuationAccess final:public GovernanceAuthorityRead {
public:
 explicit PoliticalGovernanceContinuationAccess(PoliticalGovernanceContinuationContext context):context_(std::move(context)){}
 Outcome<GovernanceAuthorityApproval> governance_authority_approval(const StableId&)const override;
 Outcome<GovernanceAuthorityApproval> governance_authority_for_acceptance(const StableId&,const StableId&,Integer)const override;
 Outcome<GovernanceContinuityResult> governance_continuity(const StableId&,const StableId&)const override;
 Outcome<GovernanceContinuityResult> governance_relief_continuity(const StableId&,const StableId&,const StableId&,const PoliticalTakeoverRead&)const override;
private: PoliticalGovernanceContinuationContext context_;
};
// 首片仅玩家签发；精确实际结果在提交尾绑定，NPC后继须另有具名producer合同。
Outcome<StableId> political_governance_continuation_new_player_source(const PoliticalGovernanceContinuationState& before,const PoliticalGovernanceContinuationState& after,const StableId& actor,Integer tick);
Status bind_political_governance_continuation_result(const CommonStore&,PoliticalGovernanceContinuationState&,const StableId& source,const StableId& actor,Integer request,const CommitResult&,const std::function<bool(const CommitResult&)>& is_done);
Status validate_political_governance_continuation_results(const CommonStore&,const PoliticalGovernanceContinuationState&,const std::function<bool(const CommitResult&)>& is_done);
BoundaryValue encode_political_continue_governance_command(const PoliticalContinueGovernance&);
Outcome<PoliticalContinueGovernance> decode_political_continue_governance_command(const BoundaryValue&);
BoundaryValue encode_political_governance_continuations(const PoliticalGovernanceContinuationState&);
Outcome<PoliticalGovernanceContinuationState> decode_political_governance_continuations(const BoundaryValue&);
Status visit_political_governance_continuations_checkpoint(CodecVisitor&,PoliticalGovernanceContinuationState&);
}
