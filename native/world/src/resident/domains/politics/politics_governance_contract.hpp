// 治理任务由S3持唯一行动钟，S5持真实支出；本域只定义冻结规则及劳动完成时的治理效果。
#pragma once
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
class CommonStore;class CodecVisitor;struct BoundaryValue;
inline constexpr const char* governance_authority_fact_kind="governance_authority_issued";
inline constexpr const char* governance_outcome_fact_kind="governance_outcome_plan";
// 作者显式给定安民条款；GDD 06 §6.8候选为6半日、钱120粮10、基础增益3、上限100。
struct AppeasementRule {
    StableId id,version,name_key,grain_item_id;
    Integer duration_ticks=0,cash_amount=0,grain_amount=0,base_support_gain=0,support_cap=0;
};
// XP与礼学级分别冻结，不能把650XP当成650级；待结算后也不按当前能力重算。
struct GovernanceOutcomePlan {
    StableId rule_id,rule_version,executor_actor_id;
    Integer work_completed_tick=0,rites_xp=0,rites_level=0,support_delta=0,support_cap=0;
};
// 有效窗和完工窗口由作者配置；玩家命令只选择策略，不提交工期或支出金额。
struct GovernanceCommissionPolicy {
    StableId id,rule_id,relief_policy_id;
    Integer offer_valid_ticks=0,completion_deadline_ticks=0;
};
// 只读批准DTO从S6不可变签发事实派生，S3只冻结authority_fact_id，S5另生成真实救济条款/预留ID。
struct GovernanceAuthorityApproval {
    StableId authority_fact_id,issuer_actor_id,office_id,grant_id,organization_id,executor_actor_id;
    StableId jurisdiction_id,site_id,rule_id,rule_version,rule_name_key,relief_policy_id;
    StableId public_wallet_id,public_inventory_id,grain_item_id,continuity_policy_id;
    Integer authority_epoch=0,jurisdiction_revision=0,issued_tick=0,offer_expires_tick=0;
    Integer work_ticks=0,completion_deadline_ticks=0,cash_limit=0,grain_limit=0;
};
// 连续性是具名政治读结果；临时停工和待改派不能冒作永久撤销或硬错误。
enum class GovernanceContinuityKind { continue_work,pause_executor,needs_reassignment,terminate_executor_dead,terminate_principal_dissolved };
struct GovernanceContinuityResult { GovernanceContinuityKind kind=GovernanceContinuityKind::continue_work;StableId code;std::optional<StableId> source_fact_id; };
// S3接受只消费真实政治批准；当前权源/执行人/世界revision复核失败保留原业务码。
class PoliticalTakeoverRead;
class GovernanceAuthorityRead {
public:
    virtual ~GovernanceAuthorityRead()=default;
    virtual Outcome<GovernanceAuthorityApproval> governance_authority_approval(const StableId& fact_id)const=0;
    virtual Outcome<GovernanceAuthorityApproval> governance_authority_for_acceptance(const StableId& fact_id,const StableId& executor,Integer expected_world_revision)const=0;
    virtual Outcome<GovernanceContinuityResult> governance_continuity(const StableId& authority_fact_id,const StableId& executor)const=0;
    virtual Outcome<GovernanceContinuityResult> governance_relief_continuity(const StableId& authority_fact_id,const StableId& executor,const StableId& relief_contract_id,const PoliticalTakeoverRead&)const{return governance_continuity(authority_fact_id,executor);}
};
Status validate_appeasement_rule(const AppeasementRule&);
// 此函数只做技能/规则快照，不证明Task完成、不分配事实，也不加民心；真实Task工作源由S3核验。
Outcome<GovernanceOutcomePlan> freeze_appeasement_outcome(const CommonStore&,const AppeasementRule&,const StableId& executor,Integer work_completed_tick,const DynamicStepInput* phase=nullptr);
// 冷恢复核冻结公式和原规则版本，不拿当前Actor技能覆盖历史劳动结果。
Status validate_governance_outcome_plan(const AppeasementRule&,const GovernanceOutcomePlan&);
BoundaryValue encode_appeasement_rule(const AppeasementRule&);
Outcome<AppeasementRule> decode_appeasement_rule(const BoundaryValue&);
BoundaryValue encode_governance_outcome_plan(const GovernanceOutcomePlan&);
Outcome<GovernanceOutcomePlan> decode_governance_outcome_plan(const BoundaryValue&);
Status visit_appeasement_rule(CodecVisitor&,AppeasementRule&);
Status visit_governance_outcome_plan(CodecVisitor&,GovernanceOutcomePlan&);
}
