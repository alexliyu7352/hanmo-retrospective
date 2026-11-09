#pragma once
#include "field_exact_contact_source.hpp"
#include "field_session_build.hpp"
namespace hanmo::resident {
// Readonly projection of the original exact contact trajectories. The same
// source serves hot preparation and historical exit-policy validation; no
// present-day roster or arbitrary caller side can reverse the original ends.
struct FieldExactSideAssignmentRead {
  std::array<Integer, 2> side_by_geometry_endpoint{};
  std::array<Integer, 2> geometry_end_by_side{};
  std::array<const StableId *, 2> faction_by_side{};
};
Outcome<FieldExactSideAssignmentRead> read_field_exact_side_assignment(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const SourceRef &actual_contact, ExactArithmeticBudget = {});
// Preparation is not permission to start a battle. The true contact command
// must establish its original encounter/attack eligibility before publishing.
Outcome<FieldSessionPreparation> prepare_field_exact_road_session(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const SourceRef &actual_contact, const StableId &battle_id,
    const StableId &field_contact_id, const StableId &player_actor_id,
    Integer battle_seed, ExactArithmeticBudget = {});
} // namespace hanmo::resident
