#pragma once
#include "budget_execution_contract.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> encode_budget_execution(const BudgetExecutionState&);
Outcome<BudgetExecutionState> decode_budget_execution(const BoundaryValue&);
Status visit_budget_execution(CodecVisitor&,BudgetExecutionState&);
class ProjectionWriter;
Status emit_budget_execution(ProjectionWriter&,const BudgetExecutionState&);
// 明示物理版，不由绑定内容推断；/1 保持旧字段序，/2 自带 schema_version=2。
Outcome<BoundaryValue> encode_budget_execution(const BudgetExecutionState&,Integer selected_version);
Outcome<BudgetExecutionState> decode_budget_execution(const BoundaryValue&,Integer selected_version);
Status visit_budget_execution(CodecVisitor&,BudgetExecutionState&,Integer selected_version);
Status emit_budget_execution(ProjectionWriter&,const BudgetExecutionState&,Integer selected_version);
}
