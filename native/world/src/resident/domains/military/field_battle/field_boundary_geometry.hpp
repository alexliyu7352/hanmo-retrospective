#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
struct FieldBattleSession;
struct FieldBattleContactRecord;
struct FieldBattleParameters;
struct FieldTerrainMetadata;
struct FieldBoundaryGeometryRead {
  const FieldBattleSession *session = nullptr;
  const FieldBattleContactRecord *origin = nullptr;
  const FieldBattleParameters *parameters = nullptr;
  WorldMoment actual_now;
  const FieldTerrainMetadata *terrain_metadata = nullptr;
  const WorldMoment *created_at = nullptr;
};
// Verified original static rectangle, not a second moving endpoint. S1 must
// intersect its single true motion prefix with the rational anchor transform;
// no contact, entry, interruption or diplomacy permission is manufactured here.
Outcome<FieldBoundaryGeometryRead> read_field_boundary_geometry(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const StableId &battle_id, ExactArithmeticBudget = {});
// Historical source cut, including completed battles. Only a real retained
// travel/movement_boundary_contact head may request this view.
Outcome<FieldBoundaryGeometryRead>
read_field_boundary_geometry_at_original_contact(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const SourceRef &actual_boundary_contact, ExactArithmeticBudget = {});
} // namespace hanmo::resident
