#include "political_group_passage_entry.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
// 原源错误保持具名失败，未实现的执行种类不伪装为拒绝通行的业务结果。
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::rule) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
}
int compare(const GroupPoliticalPassageEntryContext &x, const WorldMoment &a,
            const WorldMoment &b) {
  return take(compare_world_moments(a, b, x.arithmetic));
}
bool source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
// key仅作索引；运输编组的主体和组成版本仍逐字段参与身份核验。
bool carrier(const SpatialCarrierRef &a, const SpatialCarrierRef &b) {
  if (a.index() != b.index() ||
      spatial_carrier_key(a) != spatial_carrier_key(b))
    return false;
  if (const auto *p = std::get_if<TransportUnitCarrierRef>(&a)) {
    const auto &q = std::get<TransportUnitCarrierRef>(b);
    return p->composition_revision == q.composition_revision &&
           p->transport_unit_ref.subject.kind ==
               q.transport_unit_ref.subject.kind &&
           p->transport_unit_ref.subject.id == q.transport_unit_ref.subject.id;
  }
  return true;
}
bool execution(const MovementExecutionRef &a, const MovementExecutionRef &b) {
  const auto *p = std::get_if<GroupMovementExecution>(&a);
  const auto *q = std::get_if<GroupMovementExecution>(&b);
  return p && q && p->journey_id == q->journey_id &&
         p->segment_ordinal == q->segment_ordinal &&
         carrier(p->carrier, q->carrier);
}
// 逐字段比较原通行边界，避免与 boundary 命名空间重名。
bool same_passage_boundary(const PassageBoundaryRef &a,
                           const PassageBoundaryRef &b) {
  return a.kind == b.kind && a.id == b.id && a.map_id == b.map_id &&
         a.from_site_id == b.from_site_id && a.to_site_id == b.to_site_id;
}
bool sources(const std::vector<SourceRef> &a, const std::vector<SourceRef> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (!source(a[i], b[i]))
      return false;
  return true;
}
// 完整借用manifest比较排除热ReadStamp；真实锚来源与几何均不能由同revision替换。
bool anchor(const GroupPoliticalPassageEntryContext &x, const MovementAnchor &a,
            const MovementAnchor &b) {
  if (a.index() != b.index() || !take(same_spatial_anchor(a, b, x.arithmetic)))
    return false;
  if (const auto *p = std::get_if<OriginalMovementAnchor>(&a))
    return source(p->actual_source,
                  std::get<OriginalMovementAnchor>(b).actual_source);
  if (const auto *p = std::get_if<RoadFractionalAnchor>(&a))
    return source(p->actual_source,
                  std::get<RoadFractionalAnchor>(b).actual_source);
  const auto &p = std::get<TerrainContinuousAnchor>(a);
  const auto &q = std::get<TerrainContinuousAnchor>(b);
  return p.origin == q.origin && p.x_bits == q.x_bits && p.y_bits == q.y_bits &&
         p.geometry_profile_ref == q.geometry_profile_ref &&
         source(p.actual_prefix_close_source, q.actual_prefix_close_source);
}
bool manifest(const GroupPoliticalPassageEntryContext &x,
              const TransportManifestRead &a, const TransportManifestRead &b) {
  if (!carrier(a.carrier, b.carrier) ||
      a.manifest_revision != b.manifest_revision ||
      a.composition_revision != b.composition_revision ||
      a.at_tick != b.at_tick || !anchor(x, a.actual_anchor, b.actual_anchor) ||
      !sources(a.source_refs, b.source_refs) ||
      a.named_actor_refs != b.named_actor_refs ||
      a.vehicle_refs != b.vehicle_refs ||
      a.inventory_refs != b.inventory_refs ||
      !sources(a.civilian_population_refs, b.civilian_population_refs) ||
      !sources(a.military_force_refs, b.military_force_refs) ||
      a.persons != b.persons || a.working_crew != b.working_crew ||
      a.weight != b.weight ||
      a.vessel_placements.size() != b.vessel_placements.size())
    return false;
  for (std::size_t i = 0; i < a.vessel_placements.size(); ++i) {
    const auto &p = a.vessel_placements[i];
    const auto &q = b.vessel_placements[i];
    if (p.vessel_id != q.vessel_id ||
        !source(p.placement_source, q.placement_source) ||
        p.named_actor_ids != q.named_actor_ids ||
        p.inventory_ids != q.inventory_ids ||
        !sources(p.population_sources, q.population_sources) ||
        p.persons != q.persons || p.weight != q.weight ||
        p.maximum_persons != q.maximum_persons ||
        p.maximum_weight != q.maximum_weight)
      return false;
  }
  return true;
}
// 同M顺序来自Common原追加序；字符串ID、当前revision和时间相等都不代因果。
std::size_t position(const CommonStore &c, const SourceRef &ref) {
  const auto *h = c.state().facts.find(ref.id);
  const auto &order = c.state().facts.order();
  const auto at = std::find(order.begin(), order.end(), ref.id);
  if (!h || h->domain != ref.domain || h->kind != ref.kind || at == order.end())
    fail("PoliticalGroupEntryCommonSourceRequired", ref.id);
  return static_cast<std::size_t>(at - order.begin());
}
} // namespace
Outcome<ActualPassageEntry>
NativeGroupPoliticalPassageEntryRead::read_actual_entry(
    const PassageRequest &request,
    const TransportManifestRead &proposed) const {
  try {
    if (request.purpose != PassagePurpose::enter || !request.execution)
      fail("ActualGroupPassageEntryRequired");
    const auto *group =
        std::get_if<GroupMovementExecution>(&*request.execution);
    if (!group)
      fail("GroupPoliticalPassageEntryKindRequired", {},
           ErrorKind::unsupported_capability);
    const auto &identity = x_.common.state().identity;
    if (request.identity.world_id != identity.world_id ||
        request.identity.execution_epoch != identity.execution_epoch ||
        request.identity.revision != x_.common.revision() ||
        request.identity.tick != x_.common.tick() ||
        !carrier(request.carrier, group->carrier) ||
        !carrier(proposed.carrier, group->carrier))
      fail("PoliticalGroupEntryCandidateMismatch", group->journey_id);
    const auto now = take(x_.clock.read_organization_authority_now());
    const auto actual = take(read_actual_group_passage_entry(
        x_.carriers, x_.movement, x_.travel_catalog, *group, request.boundary,
        now, x_.arithmetic));
    const auto *captured = take(read_historical_transport_entry_manifest(
        x_.common, x_.carriers, x_.movement, x_.executions, x_.manifest_history,
        *request.execution, actual.entry_ordinal, request.boundary,
        actual.manifest_revision, actual.composition_revision, x_.arithmetic));
    if (!execution(captured->execution, *request.execution) ||
        compare(x_, captured->occurred_at, now) ||
        compare(x_, actual.occurred_at, now) ||
        !manifest(x_, captured->manifest, proposed) ||
        !take(same_spatial_anchor(actual.actual_anchor,
                                  captured->manifest.actual_anchor,
                                  x_.arithmetic)) ||
        request.manifest_basis.manifest_revision != actual.manifest_revision ||
        request.manifest_basis.composition_revision !=
            actual.composition_revision)
      fail("PoliticalGroupEntryManifestMismatch", captured->id);
    // 捕获已发生；Decision与crossing尚由原S6/S1同候选写入，不要求未来头倒证采用。
    position(x_.common, {"travel", "transport_entry_manifest", captured->id});
    return {true,
            {actual.entry_ordinal, actual.composition_revision,
             actual.manifest_revision, actual.occurred_at},
            {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status NativeGroupPoliticalPassageEntryRead::validate_historical_entry(
    const PoliticalPassageDecisionSource &supplied) const {
  try {
    const SourceRef decision_ref{"politics", "political_passage_decided",
                                 supplied.id};
    const auto *actual = take(
        read_political_passage_decision(x_.common, x_.decisions, decision_ref));
    if (actual != &supplied)
      fail("PoliticalGroupEntryStoredSourceRequired", supplied.id);
    const auto *group = std::get_if<GroupMovementExecution>(&actual->execution);
    if (!group)
      fail("GroupPoliticalPassageEntryKindRequired", actual->id,
           ErrorKind::unsupported_capability);
    const BoundaryCrossingReceipt *crossing = nullptr;
    for (const auto &row : x_.carriers.crossings.rows())
      if (source(row.second.decision_source_ref, decision_ref)) {
        if (crossing || row.first != row.second.receipt_id)
          fail("PoliticalGroupEntryCrossingNotUnique", actual->id);
        crossing = &row.second;
      }
    if (!crossing || !execution(crossing->execution, actual->execution) ||
        !carrier(crossing->carrier, actual->carrier) ||
        !carrier(actual->carrier, group->carrier) ||
        crossing->entry_ordinal != actual->entry_ordinal ||
        crossing->manifest_revision != actual->manifest_revision ||
        crossing->composition_revision != actual->composition_revision ||
        !same_passage_boundary(crossing->boundary, actual->decision.boundary) ||
        compare(x_, crossing->entry_at, actual->occurred_at) ||
        compare(x_, crossing->entry_at,
                take(x_.clock.read_organization_authority_now())) >= 0)
      fail("PoliticalGroupEntryActualCrossingRequired", actual->id);
    take(validate_group_boundary_crossing_source(
        x_.common,x_.executions,x_.carriers, x_.movement, x_.travel_catalog, *crossing, x_.arithmetic));
    const auto *captured = take(read_historical_transport_entry_manifest(
        x_.common, x_.carriers, x_.movement, x_.executions, x_.manifest_history,
        crossing->execution, crossing->entry_ordinal, crossing->boundary,
        crossing->manifest_revision, crossing->composition_revision,
        x_.arithmetic));
    if (!execution(captured->execution, actual->execution) ||
        !carrier(captured->manifest.carrier, actual->carrier) ||
        compare(x_, captured->occurred_at, actual->occurred_at))
      fail("PoliticalGroupEntryHistoricManifestMismatch", actual->id);
    const auto capture_index = position(
        x_.common, {"travel", "transport_entry_manifest", captured->id});
    const auto decision_index = position(x_.common, decision_ref);
    const auto crossing_index = position(
        x_.common, {"travel", "boundary_crossing", crossing->receipt_id});
    if (capture_index >= decision_index || decision_index >= crossing_index)
      fail("PoliticalGroupEntryCausalOrderMismatch", actual->id);
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
} // namespace hanmo::resident
