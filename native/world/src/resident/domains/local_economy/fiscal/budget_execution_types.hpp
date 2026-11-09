// E12.5F/22.1：真实责任采用预算的唯一引用，不保存第二额度余额。
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct BudgetExecutionRef { StableId budget_id; Integer budget_revision=0,policy_revision=0; StableId approval_fact_id; };
struct BudgetMoneyQian {};
struct BudgetItemQuantity { StableId item_id; };
using BudgetUnit=std::variant<BudgetMoneyQian,BudgetItemQuantity>;
struct BudgetPeriod { Integer period_from_tick=0,period_until_tick=0; };
struct BudgetChargeRef { StableId owner_domain,obligation_id,actual_leg_id;Integer charged_quantity=0;BudgetExecutionRef execution_ref; };
struct BudgetCommitmentRef { StableId owner_domain,obligation_id,terms_ref;Integer outstanding_quantity=0;BudgetExecutionRef execution_ref; };
struct BudgetReleaseRef { StableId owner_domain,obligation_id,result_id;Integer released_quantity=0;BudgetExecutionRef execution_ref; };
struct BudgetRefundRef { StableId owner_domain,original_charge_leg_id,refund_leg_id;Integer quantity=0;BudgetExecutionRef execution_ref; };
}
