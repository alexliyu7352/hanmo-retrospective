#pragma once
#include "political_budget_contract.hpp"
namespace hanmo::resident {
// Four physical source tables need not preserve cross-kind append order. This
// view reads the genuine Common order and returns original source addresses;
// it never reorders stored rows or repairs a missing/duplicate source.
Outcome<std::vector<const BudgetAuthorizationSource*>> read_public_budget_authorizations_in_actual_order(const CommonStore&,const PoliticalBudgetState&);
}
