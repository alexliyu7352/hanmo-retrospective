// 同栈只读真实委任和原批准来源，不复制Task、预算、占领或Office控制账。
#pragma once
#include "political_governance_agency_rules.hpp"
namespace hanmo::resident {
class PoliticalGovernanceAgencyAccess final:public PoliticalGovernanceAgencyRead {
 const PoliticalGovernanceAgencyContext& context_;
public:
 explicit PoliticalGovernanceAgencyAccess(const PoliticalGovernanceAgencyContext& context):context_(context){}
 PoliticalGovernanceAgencyAccess(const PoliticalGovernanceAgencyAccess&)=delete;
 PoliticalGovernanceAgencyAccess& operator=(const PoliticalGovernanceAgencyAccess&)=delete;
 Outcome<PoliticalGovernanceAgencySource> agency_for_commission(const StableId&,const StableId&)const override;
 Outcome<PoliticalGovernanceAgencySource> agency_before_issue(const StableId&,const StableId&,Integer)const override;
 Outcome<PoliticalGovernanceAgencySource> agency_for_issued_authority(const StableId&)const override;
};
}
