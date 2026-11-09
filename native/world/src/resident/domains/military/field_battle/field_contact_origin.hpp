#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
struct TravelCatalog;
// Finite original source branch: no fabricated legacy contact and no fallback.
Outcome<GeneratedBattleField> regenerate_field_contact_battle(
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldContactOrigin &,
    const FieldExactContactReadContext *exact_contacts = nullptr,
    ExactArithmeticBudget = {});
// Source-local Common provenance for the retained immutable binding. Full S1
// geometry/formation/close validation belongs to the original context above.
Status validate_field_contact_origin_head(const CommonStore &,
                                          const FieldBattleContactRecord &);
} // namespace hanmo::resident
