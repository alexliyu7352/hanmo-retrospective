// 本域只读交叉核验集中于此；没有第二份Task时钟或公共资金账。
#pragma once
#include "political_governance_continuation.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <algorithm>
namespace hanmo::resident::governance_continue_detail {
template<class C,class V> bool has(const C& c,const V& v){return std::find(c.begin(),c.end(),v)!=c.end();}
bool before(const CommonStore&,const StableId&,const StableId&);
Outcome<Integer> source_sequence(const CommonStore&,const StableId&);
Status phase(const PoliticalGovernanceContinuationContext&);
Status alive_before(const PoliticalGovernanceContinuationContext&,const StableId&,Integer,const StableId&);
Outcome<GovernanceTaskSource> task_source(const PoliticalGovernanceContinuationContext&,const StableId&,const GovernanceAuthorityApproval&,Integer,const StableId&);
Status execution_source(const PoliticalGovernanceContinuationContext&,const GovernanceAuthorityApproval&,const GovernanceTaskAcceptance&,const PoliticalResponsibilityExecution&);
Status record_source(const PoliticalGovernanceContinuationContext&,const PoliticalGovernanceContinuation&);
bool grant_covers(const Grant&,const GovernanceAuthorityApproval&,const ActingAuthority&,Integer);
Outcome<StableId> current_grant(const PoliticalGovernanceContinuationContext&,const GovernanceAuthorityApproval&,const ActingAuthority&);
Status current_assets(const PoliticalGovernanceContinuationContext&,const GovernanceAuthorityApproval&,const StableId& organization);
}
