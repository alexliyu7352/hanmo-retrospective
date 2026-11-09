#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "field_event_navigation_observation.hpp"
#include "field_event_motion_application.hpp"
#include "field_event_mission_application.hpp"
#include "field_event_contact_phase_from.hpp"
#include "resident/model/common_model.hpp"
#include "field_order_resolution.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <limits>
namespace hanmo::resident {
Status record_field_order_resolution(FieldBattleSession &s, FieldOrderRecord &o,
                                     CheckpointMemoryCharge &owner) {
  auto i = s.orders.find(o.order_id);
  if (i == s.orders.end() || &i->second != &o || o.resolution_event_sequence ||
      !o.status_at || o.status == FieldOrderStatus::InTransit ||
      o.status == FieldOrderStatus::Superseded ||
      o.status_at->tick != s.now.tick || o.status_at->t != s.now.t ||
      s.next_event_sequence <= 0 ||
      s.next_event_sequence == std::numeric_limits<Integer>::max())
    return common_failure("FieldOrderResolutionSourceRequired", o.order_id);
  auto f = s.formations.find(o.target_formation_id);
  if (f == s.formations.end())
    return common_failure("FieldOrderResolutionSourceRequired", o.order_id);
  // Reserve exact replacement shell before allocation; move existing payloads
  // without recopying their retained strings. New source strings rent first.


  (owner).grow(0);
  std::vector<FieldBattleEvent> events;
  events.reserve(s.events.size() + 1);
  FieldBattleEvent e;
  e.sequence = s.next_event_sequence;
  e.at = s.now;
  e.kind = field_event_kind::OrderResolved;
  e.source_id = checkpoint_fresh_string_copy(o.order_id);
  e.formation_id = checkpoint_fresh_string_copy(o.target_formation_id);
  e.side = f->second.side;
  for (auto &old : s.events)
    events.push_back(std::move(old));
  events.push_back(std::move(e));
  s.events.swap(events);
  o.resolution_event_sequence = s.next_event_sequence++;
  return common_success();
}
Status validate_field_order_resolution_sources(const FieldBattleSession &s) {
  for (const auto &[id, o] : s.orders) {
    if (o.status == FieldOrderStatus::InTransit) {
      if (o.resolution_event_sequence || o.resolution_boundary)
        return common_failure("FieldOrderResolutionUnexpected", id);
      continue;
    }
    if (o.status == FieldOrderStatus::Superseded ||
        !o.resolution_event_sequence || !o.resolution_boundary || !o.status_at)
      return common_failure("FieldOrderResolutionSourceRequired", id);
    if (o.resolution_boundary->last_local_event_sequence >=
        *o.resolution_event_sequence)
      return common_failure("FieldOrderResolutionBoundaryMismatch", id);
    Integer count = 0;
    for (const auto &e : s.events)
      if (e.kind == field_event_kind::OrderResolved && e.source_id == id) {
        ++count;
        auto f = s.formations.find(o.target_formation_id);
        if (e.sequence != *o.resolution_event_sequence ||
            e.at.tick != o.status_at->tick || e.at.t != o.status_at->t ||
            e.formation_id != o.target_formation_id ||
            f == s.formations.end() || e.side != f->second.side ||
            e.target_formation_id || e.count || e.tactic || e.stratagem ||
            e.success || field_event_mission_transition(e) || field_event_navigation_observation(e) ||
            field_event_motion_application(e) || field_event_mission_application(e) ||
            field_event_contact_phase_from(e))
          return common_failure("FieldOrderResolutionSourceMismatch", id);
      }
    if (count != 1)
      return common_failure("FieldOrderResolutionCoverageRequired", id);
  }
  for (const auto &e : s.events)
    if (e.kind == field_event_kind::OrderResolved &&
        (!e.source_id || !s.orders.contains(*e.source_id)))
      return common_failure("FieldOrderResolutionOrphan");
  return common_success();
}
} // namespace hanmo::resident
