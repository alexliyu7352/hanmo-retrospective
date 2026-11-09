// Original road geometry projection for Field road/off-road exits.
#pragma once
#include "field_departure_source.hpp"
namespace hanmo::resident {
struct TravelCatalog;
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
struct FieldRouteExitProjectionRead {
  FieldDepartureSourceRead departure;
  StableId route_id;
  PointI actual_map_point_dm;
  // Fraction of the original directed author polyline's integer arc length.
  // Not a WorldMoment and not an unapproved replacement MovementAnchor.
  CanonicalFraction route_fraction;
  std::size_t original_segment = 0;
};
// Pure original geometry. A Gate/village destination must still go through its
// actual settlement-exit source; callers cannot label this road projection as
// a completed S1 handoff.
Outcome<FieldRouteExitProjectionRead> read_field_route_exit_projection(
    const CommonStore &, const FieldBattleWorldState &, const TravelCatalog &,
    const FieldBattleCatalog &, const StableId &battle_id,
    const StableId &formation_id, ExactArithmeticBudget budget = {},
    const FieldExactContactReadContext *exact_contacts = nullptr);
} // namespace hanmo::resident

namespace hanmo::resident {
// 纯原地图几何，不授退出/逃散权限；调用者须另证实际source的point。
struct FieldPointRouteProjectionRead {
  StableId route_id;
  PointI actual_map_point_dm;
  CanonicalFraction route_fraction;
  std::size_t original_segment = 0;
};
Outcome<FieldPointRouteProjectionRead> read_field_point_route_projection(
    const FieldBattleWorldState &, const TravelCatalog &, const FieldBattleCatalog &,
    const StableId &battle_id, PointI actual_point_dm, ExactArithmeticBudget = {},
    const FieldExactContactReadContext *exact_contacts = nullptr);
} // namespace hanmo::resident
