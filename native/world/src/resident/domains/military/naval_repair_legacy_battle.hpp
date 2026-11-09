#pragma once
#include "naval_repair_original_history.hpp"
#include "naval_battle_history.hpp"
namespace hanmo::resident {
// Explicit original Naval2..6 battle branch. The one original battle replay
// proves every contact and closure. New DynamicMilitarySession sources require
// their own selected reader and are never inferred from these legacy rows.
Status validate_exact_naval_repair_legacy_battle_acceptance(const ExactNavalRepairHistoryContext&,const NavalHistoryAccess&,const NewNavalRepairTimeSource2&);
Status validate_exact_naval_repair_legacy_battle_span(const ExactNavalRepairHistoryContext&,const NavalHistoryAccess&,const NewNavalRepairTimeSource2&,const MomentSpan&);
Outcome<std::optional<ExactNavalRepairInterruption>> read_first_exact_naval_repair_legacy_contact(const ExactNavalRepairHistoryContext&,const NavalHistoryAccess&,const NewNavalRepairTimeSource2&);
Status validate_exact_naval_repair_legacy_battle_result(const ExactNavalRepairHistoryContext&,const NavalHistoryAccess&,const NewNavalRepairTimeSource2&);
}
