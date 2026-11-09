#pragma once
#include "movement_boundary_source.hpp"
#include <vector>
namespace hanmo::resident {
struct LocalEconomyState;
struct EconomicTransportExpenseState;
struct SpatialCarrierState;
struct GroupMovementExecutionState;
struct GroupMovementContext;
struct TravelState;
struct TravelCatalog;
class TravelServices;
class PersonalMovementFundingRead;
class BoundSpatialLocationWrite;
// Source has already been installed in the same atomic candidate at actual M.
// Reuses the original unit/payment/exertion and sole spatial writer. Any
// failure discards the enclosing candidate, never publishes an isolated source
// row.
Outcome<StableId> apply_exact_group_boundary_in_candidate(
    CommonStore &, LocalEconomyState &, EconomicTransportExpenseState &,
    SpatialCarrierState &, MovementExactState &, GroupMovementExecutionState &,
    const GroupMovementContext &, const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const SourceRef &,
    std::vector<BoundSpatialLocationWrite> &, ExactArithmeticBudget = {});
// Actual static-boundary stop has no invented second action/met_actions or
// encounter id. Field decision ownership is adopted separately in this
// candidate.
Outcome<StableId> apply_exact_personal_boundary_in_candidate(
    CommonStore &, TravelState &, MovementExactState &, const TravelCatalog &,
    const TravelServices &, const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const SourceRef &,
    std::vector<BoundSpatialLocationWrite> &,
    const PersonalMovementFundingRead * = nullptr, ExactArithmeticBudget = {});
} // namespace hanmo::resident
