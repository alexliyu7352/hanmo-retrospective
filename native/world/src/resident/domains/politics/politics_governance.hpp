// 政治治理持唯一批准与效果计划；任务状态/行动时钟归S3，钱粮和预留归S5。
#pragma once
#include "politics_model.hpp"
#include "politics_governance_contract.hpp"
#include "politics_security_types.hpp"
namespace hanmo::resident {
class TaskGovernanceRead;class PoliticalGovernanceAgencyRead;struct GovernanceAgencyIssuedAuthority;
// 规则与接单时间窗均为明确作者值，初始民心不从缺字段猜测。
struct PoliticsGovernanceCatalog {
    bool enabled=false;
    OrderedTable<AppeasementRule> appeasement_rules;
    OrderedTable<GovernanceCommissionPolicy> commission_policies;
    OrderedTable<Integer> initial_support;
};
// 具名批准是不可变历史，完整冻结成本/规则；不复制当前Task生命周期。
struct GovernanceAuthorityRecord {
    StableId id,policy_id,issuer_actor_id,office_id,grant_id,organization_id,executor_actor_id;
    StableId jurisdiction_id,site_id,relief_policy_id,public_wallet_id,public_inventory_id,continuity_policy_id;
    Integer authority_epoch=0,jurisdiction_revision=0,issued_tick=0,offer_expires_tick=0,completion_deadline_ticks=0;
    AppeasementRule rule;
};
// 同一Task工作事实最多对应一个S6计划，技能增益不散存两域。
struct RecordedGovernancePlan {
    StableId id,authority_fact_id,work_fact_id,task_id,contract_id,action_id;
    GovernanceOutcomePlan value;
};
// 民心是本辖区政治治理状态，与军队士气及人物好感不同。
struct JurisdictionCivilState {
    StableId id,registration_fact_id,last_fact_id;
    Integer support=0,revision=0;
};
// 不可变后果保存前后值与真实来源，防止税源多户或任务重试重复加减。
struct CivilConsequence {
    StableId id,jurisdiction_id,source_domain,source_fact_id,plan_id,previous_fact_id;
    Integer tick=0,before=0,after=0,requested_delta=0,revision=0;
};
struct PoliticsGovernanceState {
    OrderedTable<GovernanceAuthorityRecord> authorities;
    OrderedTable<RecordedGovernancePlan> plans;
    OrderedTable<JurisdictionCivilState> civil;
    OrderedTable<CivilConsequence> consequences;
    // 只由显式physical /2编码；/1入口保原四根字节且不接受非空新根。
    OrderedTable<JurisdictionSecurityState> jurisdiction_security;
    OrderedTable<SecurityConsequence> security_consequences;
};
// 指挥者只选择真实对象，时长/金额/事实ID全部从原生规则和共同计数器生成。
struct PoliticalGovernanceCommissionCommand {
    ActingAuthority authority;
    StableId executor_actor_id,jurisdiction_id,policy_id,public_inventory_id;
    Integer expected_jurisdiction_revision=0;
};
class PoliticsGovernanceAccess final:public GovernanceAuthorityRead {
public:
    PoliticsGovernanceAccess(const CommonStore&,const PoliticsState&,const PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&);
    PoliticsGovernanceAccess(const CommonStore&,const PoliticsState&,const PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const PoliticalGovernanceAgencyRead* agencies);
    Outcome<GovernanceAuthorityApproval> governance_authority_approval(const StableId&)const override;
    Outcome<GovernanceAuthorityApproval> governance_authority_for_acceptance(const StableId&,const StableId&,Integer expected_world_revision)const override;
    Outcome<GovernanceContinuityResult> governance_continuity(const StableId&,const StableId&)const override;
    Outcome<GovernanceContinuityResult> governance_relief_continuity(const StableId&,const StableId&,const StableId&,const PoliticalTakeoverRead&)const override;
private:
    const CommonStore& common_;const PoliticsState& politics_;const PoliticsGovernanceState& state_;const PoliticsGovernanceCatalog& catalog_;
    const OrganizationState& organization_;const OrganizationCatalog& organization_catalog_;const ActivityRead& activity_;const PoliticalGovernanceAgencyRead* agencies_=nullptr;
};
Status preview_governance_commission(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const StableId& issuer,const PoliticalGovernanceCommissionCommand&);
Outcome<GovernanceAuthorityApproval> issue_governance_authority(CommonStore&,const CommonCatalog&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const StableId& issuer,const PoliticalGovernanceCommissionCommand&);
Outcome<GovernanceAuthorityApproval> issue_governance_authority_in_transaction(CommonStore&,const CommonCatalog&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const StableId& issuer,const PoliticalGovernanceCommissionCommand&);
// 显式代理源先完整核准，原唯一writer才连续写代理见证和独占前缀批准；旧发行接口不变。
Status preview_governance_commission_for_agency(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const PoliticalGovernanceAgencyRead&,const StableId& offer_fact_id,const StableId& acceptance_fact_id,const StableId& delegate_actor,const PoliticalGovernanceCommissionCommand&);
Outcome<GovernanceAgencyIssuedAuthority> issue_governance_authority_for_agency_in_transaction(CommonStore&,const CommonCatalog&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const PoliticalGovernanceAgencyRead&,const StableId& offer_fact_id,const StableId& acceptance_fact_id,const StableId& delegate_actor,const PoliticalGovernanceCommissionCommand&);
// 下列正文与S3定版读合同衔接；没有第二个Task status或remaining_ticks。
Outcome<StableId> record_governance_outcome_plan(CommonStore&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const TaskGovernanceRead&,const StableId& work_fact_id,const DynamicStepInput&);
// 调用方持Work+Plan同一外层候选并负责完整失败回滚。
Outcome<StableId> record_governance_outcome_plan_in_transaction(CommonStore&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const TaskGovernanceRead&,const StableId& work_fact_id,const DynamicStepInput&);
BoundaryValue encode_politics_governance_state(const PoliticsGovernanceState&);
Outcome<PoliticsGovernanceState> decode_politics_governance_state(const BoundaryValue&);
Status visit_politics_governance_checkpoint(CodecVisitor&,PoliticsGovernanceState&);
BoundaryValue encode_politics_governance_catalog(const PoliticsGovernanceCatalog&);
Outcome<PoliticsGovernanceCatalog> decode_politics_governance_catalog(const BoundaryValue&);
}
