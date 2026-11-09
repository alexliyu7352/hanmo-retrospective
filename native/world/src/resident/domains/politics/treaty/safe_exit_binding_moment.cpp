#include "safe_exit_binding_moment.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
  return std::move(x.value);
}
void take(Status x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
}
int compare(const SafeExitEntryContext &x, const WorldMoment &a,
            const WorldMoment &b) {
  return take(compare_world_moments(a, b, x.arithmetic));
}
std::size_t position(const CommonStore &c, const SourceRef &ref) {
  const auto *h = c.state().facts.find(ref.id);
  const auto &order = c.state().facts.order();
  const auto it = std::find(order.begin(), order.end(), ref.id);
  if (!h || h->domain != ref.domain || h->kind != ref.kind || it == order.end())
    fail("SafeExitBindingActualSourceRequired", ref.id);
  return static_cast<std::size_t>(it - order.begin());
}
bool same_group_execution(const MovementExecutionRef &a,
                          const MovementExecutionRef &b) {
  const auto *x = std::get_if<GroupMovementExecution>(&a);
  const auto *y = std::get_if<GroupMovementExecution>(&b);
  return x && y && x->journey_id == y->journey_id &&
         x->segment_ordinal == y->segment_ordinal &&
         spatial_carrier_key(x->carrier) == spatial_carrier_key(y->carrier);
}
// 逐字段比较原通行边界，避免与 boundary 命名空间重名。
bool same_passage_boundary(const PassageBoundaryRef &a,
                           const PassageBoundaryRef &b) {
  return a.kind == b.kind && a.id == b.id && a.map_id == b.map_id &&
         a.from_site_id == b.from_site_id && a.to_site_id == b.to_site_id;
}
} // namespace
bool native_safe_exit_binding_source_kind(const FactRecord &h) {
  return h.domain == "politics" && h.kind == "safe_exit_bound";
}
Outcome<WorldMoment>
read_safe_exit_binding_source_moment(const SafeExitEntryContext &x,
                                     const MomentOwnerSource &source) {
  try {
    const auto &ref = source.source_ref;
    if (source.source_schema != 1 || !valid_source_ref(ref) ||
        !native_safe_exit_binding_source_kind({ref.id, ref.kind, ref.domain}))
      fail("SafeExitBindingSourceUnsupported", ref.id);
    const auto cut = position(x.common, ref);
    const auto *b = x.state.bindings.find(ref.id);
    if (!b || b->id != ref.id)
      fail("SafeExitBindingSourceMissing", ref.id);
    take(validate_safe_exit_binding_shape(*b));
    const auto *crossing = x.carriers.crossings.find(b->entry_receipt_ref.id);
    if (!crossing || crossing->receipt_id != b->entry_receipt_ref.id ||
        !crossing->safe_exit_binding_ref ||
        !same_source_ref(*crossing->safe_exit_binding_ref, ref) ||
        spatial_carrier_key(crossing->carrier) !=
            spatial_carrier_key(b->entry_carrier) ||
        position(x.common, b->entry_receipt_ref) >= cut)
      fail("SafeExitBindingOriginalCrossingMismatch", ref.id);
    for (const auto &row : x.state.bindings.rows())
      if (row.first != b->id &&
          same_source_ref(row.second.entry_receipt_ref, b->entry_receipt_ref))
        fail("DuplicateSafeExitEntryBinding", ref.id);
    const auto *manifest =
        x.group_execution.entry_manifests.find(b->entered_manifest_basis.id);
    if (!manifest || manifest->id != b->entered_manifest_basis.id ||
        b->entered_manifest_basis.domain != "travel" ||
        b->entered_manifest_basis.kind != "transport_entry_manifest" ||
        !same_group_execution(manifest->execution, crossing->execution) ||
        manifest->entry_ordinal != crossing->entry_ordinal ||
        !same_passage_boundary(manifest->boundary, crossing->boundary) ||
        compare(x, manifest->occurred_at, crossing->entry_at) ||
        spatial_carrier_key(manifest->manifest.carrier) !=
            spatial_carrier_key(crossing->carrier) ||
        manifest->manifest.manifest_revision != crossing->manifest_revision ||
        manifest->manifest.composition_revision !=
            crossing->composition_revision ||
        position(x.common, b->entered_manifest_basis) >=
            position(x.common, b->entry_receipt_ref))
      fail("SafeExitBindingOriginalManifestMismatch", ref.id);
    const auto positive = take(read_actual_group_crossing_first_positive_span(
        x.common, x.carriers, x.movement, x.group_execution, x.travel_catalog,
        *crossing, x.arithmetic));
    if (positive.actual_span_source.source_schema != 1 ||
        position(x.common, positive.actual_span_source.source_ref) >= cut ||
        compare(x, positive.actual_span.to,
                take(x.clock.read_organization_authority_now())) > 0)
      fail("SafeExitBindingPositiveSourceMismatch", ref.id);
    // expense本身发生在from；只有其真实采用且已积分的first_actual_span.to是本binding发生M。
    return {true, positive.actual_span.to, {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<WorldMoment>
NativeSafeExitBindingResultRead::read_selected_source_moment(
    const MomentOwnerSource &s) const {
  return read_safe_exit_binding_source_moment(context_, s);
}
Outcome<ExactOwnerFactRead>
NativeSafeExitBindingResultRead::read_selected_fact(const FactRecord &h) const {
  auto actual = read_selected_source_moment({{h.domain, h.kind, h.id}, 1});
  if (!actual.ok)
    return {false, {}, actual.error};
  ExactOwnerFactRead out;
  out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
  out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 1});
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
