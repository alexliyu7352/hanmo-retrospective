#pragma once
#include "budget_execution_contract.hpp"
#include "../material_aid/material_aid.hpp"
#include "../material_aid/material_aid_compensation2_codec.hpp"
#include "../governance_work/governance_work_expense_contract.hpp"
namespace hanmo::resident {
// The central World lease owns all referenced original providers/roots. This
// object owns its context records and concrete readers at a stable address;
// it cannot accidentally retain references to a caller's temporary contexts.
class NativeCoreBudgetResponsibilityRead final:public BudgetResponsibilityRead {
 MaterialAidReadContext aid_context_;
 GovernanceWorkExpenseReadContext work_context_;
 NativeMaterialAidBudgetRead aid_;
 GovernanceWorkBudgetResponsibilityRead work_;
 const TreatyMaterialAidState2* aid_root2_=nullptr;
 std::optional<MaterialAidCompensationReadContext2> compensation_context_;
 std::optional<NativeMaterialAidCompensationBudgetRead2> compensation_;
public:
 NativeCoreBudgetResponsibilityRead(MaterialAidReadContext aid,GovernanceWorkExpenseReadContext work)
  :aid_context_(aid),work_context_(work),aid_(aid_context_),work_(work_context_){}
 NativeCoreBudgetResponsibilityRead(MaterialAidReadContext aid,GovernanceWorkExpenseReadContext work,const TreatyMaterialAidState2&root)
  :aid_context_(aid),work_context_(work),aid_(aid_context_),work_(work_context_),aid_root2_(&root),compensation_context_(std::in_place,aid_context_,root.compensation),compensation_(std::in_place,*compensation_context_){}
 NativeCoreBudgetResponsibilityRead(const NativeCoreBudgetResponsibilityRead&)=delete;
 NativeCoreBudgetResponsibilityRead&operator=(const NativeCoreBudgetResponsibilityRead&)=delete;
 NativeCoreBudgetResponsibilityRead(NativeCoreBudgetResponsibilityRead&&)=delete;
 NativeCoreBudgetResponsibilityRead&operator=(NativeCoreBudgetResponsibilityRead&&)=delete;
 Status validate_same_job()const;
 Outcome<ActualBudgetResponsibility> read_budget_responsibility(const BudgetExecutionBinding&)const override;
 // Whole-world publication/cold gate, even with ZERO execution bindings. Do
 // not invoke from a per-binding projection: that cut must remain acyclic.
 Status validate_whole_original_coverage()const;
};
}
