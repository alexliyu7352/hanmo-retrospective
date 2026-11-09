// 有限本地页只列原治理责任，条款和可提交参数都由真实同候选预览生成。
#pragma once
#include "political_governance_continuation.hpp"
namespace hanmo::resident {
struct GovernanceTaskState;
BoundaryValue encode_political_governance_continuation_quote(const PoliticalGovernanceContinuationQuote&);
Outcome<BoundaryValue> query_political_governance_continuations(const PoliticalGovernanceContinuationContext&,const GovernanceTaskState&,const StableId& place,Integer cursor=0);
}
