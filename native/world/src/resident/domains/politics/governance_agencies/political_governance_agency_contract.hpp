// 显式治理代理仅授有限模板的新签发职责；不设置Office副手，不改变原任职/Task/公共钱粮账。
#pragma once
#include "resident/domains/politics/politics_governance.hpp"
#include "resident/domains/politics/governance_agency_intents/political_governance_agency_origin.hpp"
#include "resident/domains/politics/succession/political_succession_nominations.hpp"
namespace hanmo::resident {
inline constexpr const char* political_governance_agency_domain="political_governance_agencies";
inline constexpr const char* political_governance_agency_offer_command="political_offer_governance_agency";
inline constexpr const char* political_governance_agency_answer_command="political_answer_governance_agency";
inline constexpr const char* political_governance_agency_revoke_command="political_revoke_governance_agency";
inline constexpr const char* political_governance_agency_issue_command="political_issue_agency_governance";
// 作者同时具名代理专项权与原安民签发权；旧may_delegate单独只允许委派劳动，不能授代理签字。
struct PoliticalGovernanceAgencyPolicy {
 StableId id,jurisdiction_id,office_id,appointment_grant_id,commission_grant_id,commission_policy_id,public_inventory_id;
 Integer response_ticks=0,mandate_ticks=0,maximum_issues=0;
};
struct PoliticalGovernanceAgencyCatalog {Integer version=1;OrderedTable<PoliticalGovernanceAgencyPolicy> policies;};
// 委任冻结原首长/epoch和原辖区，任职变化或被占后不由新持有人继承旧代理。
struct PoliticalGovernanceAgencyOffer {
 StableId id,policy_id,organization_id,office_id,issuer_actor_id,delegate_actor_id,appointment_grant_id,commission_grant_id;
 Integer office_epoch=0,jurisdiction_revision=0,tick=0,response_before_tick=0,valid_through_tick=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 原候选亲自接受或谢绝；接受不创建Task、预约或治理authority。
struct PoliticalGovernanceAgencyAnswer {StableId id,offer_fact_id,actor_id;Integer tick=0;bool accept=false;std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;};
// 原权源持有人撤回只关闭未来签发，不倒销已受领承诺；他人换任本已使代理停止。
struct PoliticalGovernanceAgencyRevocation {StableId id,offer_fact_id,issuer_actor_id;Integer office_epoch=0,tick=0;std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;};
// 真正代理签发逐件关联原唯一治理批准；次数是委任范围，不是第二份预算或已支付金额。
struct PoliticalGovernanceAgencyIssue {StableId id,offer_fact_id,acceptance_fact_id,delegate_actor_id,executor_actor_id,authority_fact_id;Integer tick=0;std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;};
struct PoliticalGovernanceAgencyState {
 Integer version=1;
 OrderedTable<PoliticalGovernanceAgencyOffer> offers;
 OrderedTable<PoliticalGovernanceAgencyAnswer> answers;
 OrderedTable<PoliticalGovernanceAgencyRevocation> revocations;
 OrderedTable<PoliticalGovernanceAgencyIssue> issues;
 // 只有显式2版序列化此尾；1版必须空，原四表及必需玩家回执字节不变。
 OrderedTable<PoliticalGovernanceAgencyAuthoredOrigin> authored_origins;
};
// 指令只选择作者模板/真人和原版本，不让调用者传入金额、资源名单、权力或无限期限。
struct PoliticalOfferGovernanceAgency {ActingAuthority authority;StableId policy_id,delegate_actor_id;Integer expected_jurisdiction_revision=0;};
struct PoliticalAnswerGovernanceAgency {StableId offer_fact_id;bool accept=false;};
struct PoliticalRevokeGovernanceAgency {ActingAuthority authority;StableId offer_fact_id;};
struct PoliticalIssueAgencyGovernance {StableId offer_fact_id,acceptance_fact_id,executor_actor_id;};
using PoliticalGovernanceAgencyCommand=std::variant<PoliticalOfferGovernanceAgency,PoliticalAnswerGovernanceAgency,PoliticalRevokeGovernanceAgency,PoliticalIssueAgencyGovernance>;
// 只读有效委任保原委托人与实际签字人；原Governance批准DTO仍明确由原Office负责。
struct PoliticalGovernanceAgencySource {
 WorldIdentity source_identity;StableId offer_fact_id,acceptance_fact_id,policy_id,delegate_actor_id;
 ActingAuthority principal;StableId appointment_grant_id,commission_grant_id,jurisdiction_id,commission_policy_id,public_inventory_id;
 Integer jurisdiction_revision=0,issued_tick=0,accepted_tick=0,valid_through_tick=0,observed_tick=0,remaining_issues=0;
};
// 同次原writer返回实际代理见证及原唯一批准，暂时DTO不进入另一份预算账。
struct GovernanceAgencyIssuedAuthority {StableId signature_fact_id;GovernanceAuthorityApproval approval;};
// 未实现的消费者必须明确不可用，不能把无适配器当成有代理权限或空授权。
class PoliticalGovernanceAgencyRead {
public:
 virtual ~PoliticalGovernanceAgencyRead()=default;
 virtual Outcome<PoliticalGovernanceAgencySource> agency_for_commission(const StableId& offer_fact_id,const StableId& actor_id)const=0;
 virtual Outcome<PoliticalGovernanceAgencySource> agency_before_issue(const StableId& offer_fact_id,const StableId& issue_fact_id,Integer tick)const=0;
 // 只认独占代理批准前缀，缺对应真实签发来源时不能退回原首长自签历史。
 virtual Outcome<PoliticalGovernanceAgencySource> agency_for_issued_authority(const StableId& authority_fact_id)const=0;
};
Status validate_political_governance_agency_catalog(const CommonStore&,const PoliticsCatalog&,const PoliticsGovernanceCatalog&,const OrganizationCatalog&,const PoliticalGovernanceAgencyCatalog&);
// 只检查本段精确三元头和正实例；不把它单独当作完整生命、本人、任职或代理有效性证明。
std::optional<Integer> political_governance_agency_fact_sequence(const CommonStore&,const StableId&,const StableId& expected_kind);
}
