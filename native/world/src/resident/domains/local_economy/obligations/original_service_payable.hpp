#pragma once
#include "obligation_sources_read.hpp"
namespace hanmo::resident {
struct GovernanceWorkExpenseReadContext;struct RepairServiceReadContext;
// The full original earned predicate/accepted terms are checked here. The
// original debt, cash and P11 tables are deliberately outside this source cut.
Outcome<OriginalEconomicPayable> read_original_work_charge_for_obligation(
 const GovernanceWorkExpenseReadContext&,const MomentOwnerSource&);
Outcome<OriginalEconomicPayable> read_original_repair_charge_for_obligation(
 const RepairServiceReadContext&,const MomentOwnerSource&);
}
