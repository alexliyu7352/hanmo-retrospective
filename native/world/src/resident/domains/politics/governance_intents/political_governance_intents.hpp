// 作者一次安民意图只选原双方和委任策略；实际签发、本人受领与公共资源仍由原三域命令执行。
#pragma once
#include "resident/domains/politics/politics_governance.hpp"
namespace hanmo::resident {
inline constexpr const char* political_governance_intents_module="political_governance_intents";
inline constexpr const char* political_governance_intent_template="governance_commission";
// 双方分别受原Director预算调度；一份既有匹配邀请即消耗本次签发意图，不因过期自动重发。
struct PoliticalGovernanceIntent {
 StableId id,issuer_actor_id,executor_actor_id,office_id,jurisdiction_id,commission_policy_id,public_inventory_id;
};
// 此目录没有运行账；政治批准及Task受领原事实是唯一可用完成来源。
struct PoliticalGovernanceIntentCatalog {Integer version=1;OrderedTable<PoliticalGovernanceIntent> intents;};
Outcome<PoliticalGovernanceIntentCatalog> decode_political_governance_intents(const BoundaryValue&);
BoundaryValue encode_political_governance_intents(const PoliticalGovernanceIntentCatalog&);
Status validate_political_governance_intents(const PoliticalGovernanceIntentCatalog&,const CommonCatalog&,const PoliticsCatalog&,const PoliticsGovernanceCatalog&,const OrganizationCatalog&);
}
