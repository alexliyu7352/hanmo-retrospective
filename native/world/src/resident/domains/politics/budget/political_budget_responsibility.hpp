#pragma once
#include "resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp"
#include "resident/domains/local_economy/material_aid/material_aid.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 仅借两个原责任owner，统一支用投影始终遍历同一bindings根。
class PublicBudgetResponsibilityRead final : public BudgetResponsibilityRead {
public:
  PublicBudgetResponsibilityRead(const WorldBundle &,
                                 const BudgetExecutionState &,
                                 const MaterialAidReadContext &,
                                 const GovernanceWorkExpenseReadContext &);
  Outcome<ActualBudgetResponsibility>
  read_budget_responsibility(const BudgetExecutionBinding &) const override;
  Status validate_roots() const;

private:
  const WorldBundle &world_;
  const BudgetExecutionState &execution_;
  const MaterialAidReadContext &aid_;
  const GovernanceWorkExpenseReadContext &work_;
  NativeMaterialAidBudgetRead aid_read_;
  GovernanceWorkBudgetResponsibilityRead work_read_;
};
} // namespace hanmo::resident
