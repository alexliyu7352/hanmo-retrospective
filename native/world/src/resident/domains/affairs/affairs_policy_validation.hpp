// 需求政策的纯条款和最大单合同承诺；不证明Office、Grant、资金归属或来源知情。
#pragma once
#include "affairs_model.hpp"
namespace hanmo::resident {
// 每项金额均来自原最大认领份额；合计只作授权上限，不建立余额或预约。
struct NeedPolicyCommitment { Integer purchase_budget_cap=0,reward_total=0,total=0; };
Status validate_need_policy(const NeedPolicy&);
Outcome<NeedPolicyCommitment> quote_need_policy_commitment(const NeedPolicy&);
}
