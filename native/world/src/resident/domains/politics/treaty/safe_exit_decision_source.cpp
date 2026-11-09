#include "safe_exit_decision_source.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
bool same_boundary(const PassageBoundaryRef &a, const PassageBoundaryRef &b) {
  return a.kind == b.kind && a.id == b.id && a.map_id == b.map_id &&
         a.from_site_id == b.from_site_id && a.to_site_id == b.to_site_id;
}
bool same_execution(const MovementExecutionRef &a,
                    const MovementExecutionRef &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *p = std::get_if<PersonalMovementExecution>(&a)) {
    const auto &q = std::get<PersonalMovementExecution>(b);
    return p->action_id == q.action_id && p->journey_id == q.journey_id &&
           p->segment_ordinal == q.segment_ordinal;
  }
  if (const auto *p = std::get_if<GroupMovementExecution>(&a)) {
    const auto &q = std::get<GroupMovementExecution>(b);
    return p->journey_id == q.journey_id &&
           p->segment_ordinal == q.segment_ordinal &&
           spatial_carrier_key(p->carrier) == spatial_carrier_key(q.carrier);
  }
  return false;
}
} // namespace
Outcome<const PoliticalPassageDecisionSource *>
read_safe_exit_crossing_decision_source(
    const SafeExitDecisionSourceContext &x,
    const BoundaryCrossingReceipt &crossing) {
  auto source = read_political_passage_decision(x.common, x.decisions,
                                                crossing.decision_source_ref);
  if (!source.ok)
    return source;
  const auto &r = *source.value;
  if (r.decision.admission != PassageAdmission::allowed ||
      !same_execution(r.execution, crossing.execution) ||
      spatial_carrier_key(r.carrier) != spatial_carrier_key(crossing.carrier) ||
      r.entry_ordinal != crossing.entry_ordinal ||
      r.composition_revision != crossing.composition_revision ||
      r.manifest_revision != crossing.manifest_revision ||
      !same_boundary(r.decision.boundary, crossing.boundary))
    return {false, nullptr,
            common_failure("SafeExitActualDecisionEntryMismatch",
                           crossing.receipt_id)
                .error};
  auto same_at = compare_world_moments(r.occurred_at, crossing.entry_at);
  if (!same_at.ok)
    return {false, nullptr, same_at.error};
  if (same_at.value)
    return {false, nullptr,
            common_failure("SafeExitActualDecisionMomentMismatch",
                           crossing.receipt_id)
                .error};
  const auto *head = x.common.state().facts.find(crossing.receipt_id);
  const auto &order = x.common.state().facts.order();
  const auto before = std::find(order.begin(), order.end(), r.id),
             after = std::find(order.begin(), order.end(), crossing.receipt_id);
  if (!head || head->domain != "travel" || head->kind != "boundary_crossing" ||
      before == order.end() || after == order.end() || before >= after)
    return {false, nullptr,
            common_failure("SafeExitDecisionCausalSourceRequired",
                           crossing.receipt_id)
                .error};
  auto valid = x.entries.validate_historical_entry(r);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  return source;
}
Outcome<const PoliticalPassageDecisionSource *>
read_safe_exit_crossing_decision(const SafeExitDecisionSourceContext &x,
                                 const BoundaryCrossingReceipt &crossing) {
  auto source = read_safe_exit_crossing_decision_source(x, crossing);
  if (!source.ok)
    return source;
  auto valid = x.history.validate_historical_decision(*source.value);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  return source;
}
// 实际条款分类缺决定中已采用的条约权限依据；当前入口显式拒绝，不补造持久字段或改写候选。
Outcome<std::optional<SafeExitEntryTerm>>
read_safe_exit_entry_term_from_actual_decision(
    const SafeExitDecisionSourceContext &, const BoundaryCrossingReceipt &) {
  return {false,
          {},
          common_failure("NOT_IMPLEMENTED",
                         "politics/treaty/"
                         "safe_exit_decision_source.cpp::read_safe_exit_entry_"
                         "term_from_actual_decision",
                         ErrorKind::unsupported_capability)
              .error};
}

} // namespace hanmo::resident
