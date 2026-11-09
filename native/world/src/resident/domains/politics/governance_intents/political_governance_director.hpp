// 已到期原主体的有限提案；不会扫描远方实时人员、另起预算或直接写入任何领域。
#pragma once
#include "political_governance_intents.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
struct GovernanceReadContext;
Outcome<std::optional<DecisionProposal>> propose_director_governance_commission(
 const GovernanceReadContext&,const TravelCatalog&,const KnownSubjectView&,const DirectorPolicy&,
 const PoliticalGovernanceIntent&,const DecisionProposal&,Integer candidate_limit);
// 冷完成来源只读真实原批准/受领，后来职权失效不抹掉当时已完成的提案。
Status validate_governance_intent_completed_source(const GovernanceReadContext&,const PoliticalGovernanceIntent&,
 const StableId& actor_id,const StableId& fact_id,Integer observed_tick);
}
