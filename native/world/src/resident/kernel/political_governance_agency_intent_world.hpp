// 内部NPC命令只借当前明确策略；客户端不能从普通四命令构造此包装。
#pragma once
#include "political_governance_agency_world.hpp"
#include "resident/domains/politics/governance_agency_intents/political_governance_agency_authored.hpp"
#include "resident/domains/politics/governance_agency_intents/political_governance_agency_intent_validation.hpp"
namespace hanmo::resident {
// 持者与原Governance context在同一短栈覆盖完整提案/预览，不留下借用引用。
Outcome<std::optional<DecisionProposal>> propose_political_governance_agency_director_world(const WorldBundle&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Status validate_political_governance_agency_director_world(const WorldBundle&);
Outcome<PoliticalGovernanceAgencyQuote> prepare_authored_governance_agency_world(const WorldBundle&,const StableId&,const AuthoredGovernanceAgencyCommand&);
Outcome<PoliticsEffects> commit_authored_governance_agency_world(WorldBundle&,const StableId&,const AuthoredGovernanceAgencyCommand&,const PoliticalGovernanceAgencyQuote&);
}
