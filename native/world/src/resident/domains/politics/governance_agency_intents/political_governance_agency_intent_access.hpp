// 同栈只读原代理和原Task，不把提案/调度状态当作公权或另一份接受账。
#pragma once
#include "political_governance_agency_director.hpp"
#include "resident/domains/politics/governance_agencies/political_governance_agency_business.hpp"
#include "resident/domains/tasks/governance_task_rules.hpp"
namespace hanmo::resident {
struct GovernanceReadContext;
class PoliticalGovernanceAgencyIntentAccess final:public PoliticalGovernanceAgencyIntentRead {
public:
 PoliticalGovernanceAgencyIntentAccess(const PoliticalGovernanceAgencyBusinessContext&,const GovernanceReadContext&,StableId selected_policy);
 Outcome<PoliticalGovernanceAgencyKnownIntent> agency_intent_known(const StableId&,const PoliticalGovernanceAgencyIntent&)const override;
 Outcome<std::shared_ptr<const CommandPayload>> preview_agency_intent_local(const StableId&,const PoliticalGovernanceAgencyIntent&,const PoliticalGovernanceAgencyKnownIntent&)const override;
 // current/archive的真实复评截面必须由Director原账提供；不拿今日tick填旧完成。
 Status completed_before_review(const PoliticalGovernanceAgencyIntent&,const StableId& actor,const StableId& actual_fact,Integer reviewed_tick)const;
private:
 const PoliticalGovernanceAgencyBusinessContext& business_;const GovernanceReadContext& governance_;GovernanceTaskRead tasks_;StableId selected_policy_;
};
}
