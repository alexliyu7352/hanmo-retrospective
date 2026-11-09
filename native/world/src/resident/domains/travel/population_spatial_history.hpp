// S5旧location
// revision所指真实S1来源在给定服务span的空间读面；不保存人口或第二位置。
#pragma once
#include "group_transport_registration.hpp"
namespace hanmo::resident {
struct PopulationHistoricalLocationPoint {
  CivilianPopulationRef population_ref;
  SourceRef location_source_ref, actual_adoption_source;
  WorldMoment observed_at;
  MovementAnchor actual_anchor;
  std::vector<MomentOwnerSource> original_sources;
};
// The original adopting owner verifies actual_adoption_source and its exact M.
// Same-M placement changes after that real Common append cut are excluded.
Outcome<PopulationHistoricalLocationPoint> read_population_location_at_source(
    const CommonStore &, const SpatialCarrierState &,
    const MovementExactState &, const TransportRegistrationState &,
    const PopulationTransportState &, const CivilianPopulationRef &,
    const SourceRef &location_source_ref, const WorldMoment &actual_at,
    const SourceRef &actual_adoption_source, ExactArithmeticBudget = {});
// 只供热命令 preflight：必须是当前真实 Common 全前缀及原 clock 的当前 M。
// 返回纯实际位置，不返回/伪造尚不存在的 adoption source，不能用作 cold 历史口。
struct CommonActualFactPrefix;
class CurrentMomentRead;
Outcome<MovementAnchor> read_population_location_at_actual_fact_prefix(
    const CommonStore &, const SpatialCarrierState &,
    const MovementExactState &, const TransportRegistrationState &,
    const PopulationTransportState &, const CivilianPopulationRef &,
    const SourceRef &location_source_ref, const WorldMoment &actual_at,
    const CommonActualFactPrefix &, const CurrentMomentRead &,
    ExactArithmeticBudget = {});
struct PopulationStationaryLocationSpan {
  SourceRef location_source_ref;
  MomentSpan span;
  MovementAnchor actual_anchor;
  std::vector<MomentOwnerSource> original_sources;
};
Outcome<PopulationStationaryLocationSpan>
read_population_stationary_location_span(
    const CommonStore &, const SpatialCarrierState &,
    const MovementExactState &, const TransportRegistrationState &,
    const PopulationTransportState &, const CivilianPopulationRef &,
    const SourceRef &location_source_ref, const MomentSpan &,
    ExactArithmeticBudget budget = {});
} // namespace hanmo::resident
