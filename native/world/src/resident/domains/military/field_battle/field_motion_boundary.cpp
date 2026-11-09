#include "field_motion_boundary.hpp"
#include "field_boundary_install.hpp"
#include "field_boundary_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/travel/movement_boundary_apply.hpp"
#include "resident/domains/travel/movement_boundary_geometry.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <iterator>
namespace hanmo::resident {
namespace {
using namespace movement_detail;
// 当前真实运动单位与所有活跃场界求最早交点；原几何 reader 自验原地形和时钟。
std::optional<MovementBoundaryPrediction> first_boundary(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const StableId &prefix, ExactArithmeticBudget b) {
  std::optional<MovementBoundaryPrediction> result;
  std::size_t count = 0;
  for (const auto &[id, session] : x.field.sessions) {
    if (session.state == FieldBattleState::Ended)
      continue;
    if (++count > b.max_steps)
      fail("FieldBoundaryPredictionBudgetExceeded", prefix);
    auto next = take(predict_movement_field_boundary(x, catalog, id, prefix, b));
    if (!next)
      continue;
    const auto order = result ? take(compare_world_moments(
                                    next->contact.occurred_at,
                                    result->contact.occurred_at, b)) : -1;
    if (!result || order < 0 ||
        (order == 0 && id < result->contact.original_field_source.id))
      result = std::move(next);
  }
  return result;
}
} // namespace
Outcome<std::vector<ExactEventPrediction>> predict_original_field_boundaries(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const WorldMoment &from, const WorldMoment &to, ExactArithmeticBudget b) {
  return movement_detail::guard<std::vector<ExactEventPrediction>>([&] {
    using namespace movement_detail;
    if (!x.clock.bound_to(x.common, x.military, x.field) ||
        !same_moment(take(x.clock.read_current_moment()), from, b) ||
        take(compare_world_moments(from, to, b)) > 0)
      fail("FieldBoundaryPredictionActualClockRequired");
    std::vector<ExactEventPrediction> result;
    std::size_t count = 0;
    for (const auto &row : x.movement.prefixes.rows()) {
      const auto &prefix = row.second;
      if (!std::holds_alternative<MovementActive>(prefix.lifecycle))
        continue;
      if (++count > b.max_steps)
        fail("FieldBoundaryPredictionBudgetExceeded", row.first);
      auto next = first_boundary(x, catalog, row.first, b);
      if (next && take(compare_world_moments(next->contact.occurred_at, to, b)) <= 0)
        result.push_back({next->contact.occurred_at, ExactStage::same_moment_hits,
                          {"travel", "movement_unit", prefix.unit_ref}, 1,
                          x.common.revision()});
    }
    std::sort(result.begin(), result.end(), [&](const auto &a, const auto &z) {
      const auto order = take(compare_world_moments(a.at, z.at, b));
      return order ? order < 0 : a.actual_source_basis.id < z.actual_source_basis.id;
    });
    return result;
  });
}
Status validate_original_field_boundary_application(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const ExactEventApplied &event,
    ExactArithmeticBudget b) {
  return movement_detail::status([&] {
    using namespace movement_detail;
    const auto &source = event.applied_result;
    const auto row = x.field.boundary_contacts.find(source.id);
    if (event.source_schema != 1 || source.domain != "military" ||
        source.kind != "field_battle_contact" ||
        row == x.field.boundary_contacts.end())
      fail("FieldBoundaryActualApplicationRequired", source.id);
    take(validate_field_exact_boundary_contacts(x, relations, catalog, b));
  });
}
Outcome<ExactEventApplied> apply_original_field_boundary_in_candidate(
    MotionExactWriteBinding &w, FieldBattleWorldState &field,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const ExactEventPrediction &event,
    ExactPhaseWriteContext &capture, CheckpointMemoryCharge &memory,
    ExactArithmeticBudget b) {
  return movement_detail::guard<ExactEventApplied>([&] {
    using namespace movement_detail;
    const auto &basis = event.actual_source_basis;
    if (&w.common != &x.common || &w.movement != &x.movement ||
        &field != &x.field || event.stage != ExactStage::same_moment_hits ||
        basis.domain != "travel" || basis.kind != "movement_unit" ||
        event.source_schema != 1 ||
        !same_moment(take(w.current_moment.read_current_moment()), event.at, b) ||
        !same_moment(take(x.clock.read_current_moment()), event.at, b))
      fail("FieldBoundaryActualEventRequired", basis.id);
    const MovementPrefixState *prefix = nullptr;
    for (const auto &row : w.movement.prefixes.rows())
      if (row.second.unit_ref == basis.id &&
          std::holds_alternative<MovementActive>(row.second.lifecycle)) {
        if (prefix)
          fail("FieldBoundaryActualUnitAmbiguous", basis.id);
        prefix = &row.second;
      }
    if (!prefix)
      fail("FieldBoundaryActualUnitRequired", basis.id);
    auto prediction = first_boundary(x, catalog, prefix->id, b);
    if (!prediction || !same_moment(prediction->contact.occurred_at, event.at, b))
      fail("StaleFieldBoundaryEvent", basis.id);
    const bool personal =
        std::holds_alternative<PersonalMovementExecution>(prefix->execution);
    const auto source = take(install_movement_boundary_contact_in_candidate(
        w.common, w.movement, x, catalog, *prediction, b));
    StableId exertion;
    if (personal) {
      exertion = take(apply_exact_personal_boundary_in_candidate(
          w.common, w.travel, w.movement, w.travel_catalog, w.travel_services,
          x, catalog, source, capture.spatial_location_writes,
          w.personal_funding, b));
    } else {
      if (!w.group_context)
        fail("FieldBoundaryOriginalGroupRequired", basis.id);
      exertion = take(apply_exact_group_boundary_in_candidate(
          w.common, w.economy, w.payments, w.carriers, w.movement,
          w.group_executions, *w.group_context, x, catalog, source,
          capture.spatial_location_writes, b));
    }
    if (!w.consume_actual_spatial_inputs_in_candidate)
      fail("MotionActualSpatialInputConsumerRequired");
    take(w.consume_actual_spatial_inputs_in_candidate(capture));
    if (!exertion.empty()) {
      auto condition = take(apply_travel_exertion_condition(
          w.common, w.foundation, w.actor_catalog, w.movement, exertion,
          w.condition_sources, w.capability_changes));
      capture.condition_writes.insert(
          capture.condition_writes.end(),
          std::make_move_iterator(condition.actual_writes.begin()),
          std::make_move_iterator(condition.actual_writes.end()));
    }
    const auto installed = take(install_field_boundary_contact_in_candidate(
        w.common, field, x, relations, catalog, source, memory, b));
    ExactEventApplied result{{"military", "field_battle_contact", installed}, 1};
    take(validate_original_field_boundary_application(x, relations, catalog,
                                                       result, b));
    return result;
  });
}
} // namespace hanmo::resident
