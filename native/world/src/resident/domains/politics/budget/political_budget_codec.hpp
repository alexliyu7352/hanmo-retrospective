#pragma once
#include "political_budget_contract.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_political_budgets(CodecVisitor &, PoliticalBudgetState &);
Status visit_political_budgets(CodecVisitor &, PoliticalBudgetState &,
                               Integer selected_version);
Status emit_political_budgets_projection(ProjectionWriter &, std::string_view,
                                         const PoliticalBudgetState &);
Status emit_political_budgets_projection(ProjectionWriter &, std::string_view,
                                         const PoliticalBudgetState &,
                                         Integer selected_version);
Outcome<BoundaryValue> encode_political_budgets(const PoliticalBudgetState &);
Outcome<BoundaryValue> encode_political_budgets(const PoliticalBudgetState &,
                                                Integer selected_version);
Outcome<PoliticalBudgetState> decode_political_budgets(const BoundaryValue &);
Outcome<PoliticalBudgetState>
decode_political_budgets(const BoundaryValue &, Integer selected_version);
} // namespace hanmo::resident
