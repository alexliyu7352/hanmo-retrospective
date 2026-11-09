// Original S9 terminal and S7 financial sources, projected without new state.
#pragma once
#include "service_actual_termination_result.hpp"
#include "resident/domains/craft/repair_service_sources.hpp"
#include "resident/domains/commerce/repair_service/repair_service_financial_sources.hpp"
namespace hanmo::resident {
// The canonical termination ID is the existing S9 interruption source ID.
// This read neither settles money nor releases/returns an item.
// Returns the explicit no-advance result /4; nested service identities stay /3.
Outcome<OwnedServiceActualTerminationResult> read_repair_service_actual_termination(
 const RepairReadContext&,const RepairServiceReadContext&,
 const RepairService&,const RepairTerminal&);
}
