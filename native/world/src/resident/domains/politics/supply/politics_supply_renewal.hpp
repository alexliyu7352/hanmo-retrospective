// 显式经营意图只续签已裁定责任；调度预算不是首次接管的整份处置授权。
#pragma once
#include "politics_supply_adoptions.hpp"
#include "resident/domains/local_economy/supply/supply_policy_rules.hpp"
namespace hanmo::resident {
// 作者逐条指向原接管规则和本域采办方案，不接受任意账户或默认处置其它责任。
struct PoliticalSupplyRenewalIntent {
    StableId id,takeover_rule_id,author_policy_id;
};
struct PoliticalSupplyRenewalProposal {
    StableId registration_fact_id,previous_approval_fact_id,policy_fact_id,wait_code;
    std::variant<std::monostate,IssueEconomicSupplyPolicyCommand,PoliticalTakeoverDecisionCommand> command;
};
// 纯提案只引用同候选；返回原具名命令后仍须中央真实重报价和原子采用。
Outcome<PoliticalSupplyRenewalProposal> propose_political_supply_renewal(
    const PoliticalTakeoverContext&,const EconomicSupplyPolicyContext&,
    const StableId& actor,const PoliticalSupplyRenewalIntent&,const StableId& registration_fact_id);
}
