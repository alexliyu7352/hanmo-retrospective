#pragma once
#include "naval_repair_model.hpp"
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
// One original C21 material/escrow recipe, used by both selected clock profiles.
// Original offer/completion qualification is checked by the respective owner
// before calling. These builders grant no permission and perform no mutation.
// Optional original service payment is appended first by the one charge owner.
// The sole Common resource executor retains quantity, ownership, reservation,
// capacity, checked arithmetic and all-or-nothing resource mutation semantics.
void append_original_naval_repair_material_reservation(ResourceRecipe&,
    const NavalRepairTerms&,const StableId& actual_action_id);
void append_original_naval_repair_material_terminal(ResourceRecipe&,
    const NavalRepairTerms&,const StableId& actual_action_id,bool actual_success);
}
