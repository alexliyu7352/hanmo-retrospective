// 作者只声明三名真实当事人的一次办理意向，Office、代理答受与原安民批准仍各守原账。
#pragma once
#include "resident/domains/politics/governance_agencies/political_governance_agency_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_governance_agency_intents_module="political_governance_agency_intents";
inline constexpr const char* political_governance_agency_intent_template="governance_agency";
// 首片只三个不同本人；一份原委任消耗一次提案，拒绝、撤权或到期都不自动另发新约。
struct PoliticalGovernanceAgencyIntent {
 StableId id,agency_policy_id,principal_actor_id,delegate_actor_id,executor_actor_id;
 StableId principal_director_policy_id,delegate_director_policy_id,executor_director_policy_id;
 bool delegate_accepts=false,executor_accepts=false;
};
struct PoliticalGovernanceAgencyIntentCatalog {Integer version=1;OrderedTable<PoliticalGovernanceAgencyIntent> intents;};
// 此来源只用于显式agency2：不能删除v1必需的玩家结果，也不以公平调度预算代替政治授权。
Outcome<PoliticalGovernanceAgencyIntentCatalog> decode_political_governance_agency_intents(const BoundaryValue&);
BoundaryValue encode_political_governance_agency_intents(const PoliticalGovernanceAgencyIntentCatalog&);
Outcome<std::optional<PoliticalGovernanceAgencyIntentCatalog>> decode_political_governance_agency_intent_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
// 先核immutable目录引用；真实运行还必须经过agency2完整Grant/成员/生命/当面预览。
Status validate_political_governance_agency_intents(const PoliticalGovernanceAgencyIntentCatalog&,const CommonCatalog&,const PoliticsCatalog&,const OrganizationCatalog&,const PoliticalGovernanceAgencyCatalog&);
// 仅按显式意图匹配实际actor和阶段，不读取远方位置、今天Office或任何Task状态。
Status validate_political_governance_agency_authored_origin(const PoliticalGovernanceAgencyIntentCatalog&,const PoliticalGovernanceAgencyAuthoredOrigin&,const StableId& actor_id,const StableId& command_kind);
}
