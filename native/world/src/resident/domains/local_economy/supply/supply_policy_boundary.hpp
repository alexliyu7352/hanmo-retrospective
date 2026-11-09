// 具名新政策命令与有限报价；界面不得传入冻结条款、批准事实或库存数。
#pragma once
#include "supply_policy_rules.hpp"
namespace hanmo::resident {
Outcome<IssueEconomicSupplyPolicyCommand> decode_issue_economic_supply_policy(const BoundaryValue&);
BoundaryValue encode_issue_economic_supply_policy(const IssueEconomicSupplyPolicyCommand&);
BoundaryValue encode_economic_supply_policy_quote(const EconomicSupplyPolicyQuote&);
BoundaryValue encode_economic_supply_policy_receipt(const EconomicSupplyPolicyReceipt&);
Outcome<std::optional<DynamicFactView>> economic_supply_policy_dynamic_fact(const EconomicSupplyPolicyContext&,const StableId&);
}
