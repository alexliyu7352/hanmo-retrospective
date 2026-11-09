// 提案只消费本人已知旧源与原当地报价；真正接入前没有新增writer或隐式NPC许可。
#pragma once
#include "political_governance_agency_intents.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
struct PoliticalGovernanceAgencyKnownIntent {
 WorldIdentity identity;StableId observer_actor_id,intent_id;
 // 原三名分别只读自己亲历来源，空值不是对远方答复/签发/受领的否定证明。
 StableId known_offer_fact_id,own_answer_fact_id,own_issue_fact_id,known_authority_fact_id,own_task_acceptance_fact_id;
 Integer offer_response_before_tick=0,mandate_valid_through_tick=0,authority_offer_expires_tick=0;
 bool own_answer_accepts=false;
};
class PoliticalGovernanceAgencyIntentRead {
public:
 virtual ~PoliticalGovernanceAgencyIntentRead()=default;
 virtual Outcome<PoliticalGovernanceAgencyKnownIntent> agency_intent_known(const StableId& actor,const PoliticalGovernanceAgencyIntent&)const=0;
 // 只在本人实际到作者原座所后调用。返回经原领域完整preview的原CommandPayload，不替换玩家或绕过Grant。
 virtual Outcome<std::shared_ptr<const CommandPayload>> preview_agency_intent_local(const StableId& actor,const PoliticalGovernanceAgencyIntent&,const PoliticalGovernanceAgencyKnownIntent&)const=0;
};
struct PoliticalGovernanceAgencyDirectorContext {
 const CommonStore& common;const TravelCatalog& travel;const PoliticsCatalog& politics;const PoliticalGovernanceAgencyCatalog& agencies;
 const PoliticalGovernanceAgencyIntentRead& known;StableId selected_policy_id;
};
Outcome<std::optional<DecisionProposal>> propose_director_governance_agency(const PoliticalGovernanceAgencyDirectorContext&,const KnownSubjectView&,const DirectorPolicy&,const PoliticalGovernanceAgencyIntent&,const DecisionProposal&,Integer candidate_limit);
}
