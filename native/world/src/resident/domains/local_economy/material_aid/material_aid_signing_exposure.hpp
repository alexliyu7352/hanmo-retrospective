// Sign-before pure projection of the original finite Aid term. No Schedule,
// current budget cap, price conversion or second monetary ledger is involved.
#pragma once
#include "material_aid.hpp"
namespace hanmo::resident {
struct MaterialAidSigningMeasure {
 BudgetUnit unit;Integer per_occurrence=0,all_occurrences=0;
};
struct MaterialAidSigningExposure {
 Integer occurrences=0;
 std::vector<MaterialAidSigningMeasure> ordinary;
 // A deadline is unique per AidOccurrenceKey, not per whole schedule. A zero
 // maximum still has an explicit selected route but creates no zero payable.
 std::optional<MaterialAidSigningMeasure> bounded_compensation;
};
// Caller authenticates the immutable original term/revision and explicit route
// selection; mapping units to selected source containers remains with S6's
// original signing source. Ordinary and compensation qian components combine
// with checked addition for one original Grant unit, not for a new debt.
Outcome<MaterialAidSigningExposure> read_material_aid_signing_exposure(
 const TreatyMaterialAidTerm& original_term);
}
