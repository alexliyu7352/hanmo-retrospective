// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原采用 cut 的无环反读；只证明原条款、身份与成本顺序，不冒完整开工准入。
#include "facility_construction_adoption.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
void original_cost(const FacilityConstructionAdoptionContext &context,
                   const FacilityConstruction &construction,
                   const ConstructionTerms &terms, std::size_t start_order) {
  const ConstructionStartCost *cost = nullptr;
  for (const auto &candidate : context.state.start_costs)
    if (candidate.receipt_id == construction.actual_cost_ref) {
      if (cost)
        fail("DuplicateConstructionCost", construction.actual_cost_ref);
      cost = &candidate;
    }
  const auto *asset = context.assets.assets.find(terms.asset_ref);
  if (!cost || cost->receipt_id != construction.actual_cost_ref ||
      cost->construction_id != construction.construction_id || !asset ||
      compare(cost->occurred_at, construction.started_at))
    fail("ConstructionOriginalCostMismatch", construction.construction_id);
  // 原热捕获所留的完整有限前后帧证明实际耗料/钱腿。不再要求历史容器
  // 今天仍存在或当时Owner今天仍有权，原当时产权与用途另走真实权源门。
  take(validate_facility_construction_start_cost_effects(terms, *cost));
  const auto cost_order = take(read_actual_common_fact_position(
      context.common, facility_construction_cost_source(cost->receipt_id)));
  if (cost_order >= start_order)
    fail("ConstructionOriginalCostOrderMismatch", cost->receipt_id);
  for (const auto &row : context.state.constructions)
    if (row.construction_id != construction.construction_id &&
        row.actual_cost_ref == construction.actual_cost_ref)
      fail("ConstructionCostReused", cost->receipt_id);
}
} // namespace

Outcome<const FacilityConstruction *>
read_facility_construction_adoption_for_original_dependency(
    const FacilityConstructionAdoptionContext &context,
    const MomentOwnerSource &source) {
  try {
    if (context.state.schema_version != 1 || source.source_schema != 1 ||
        source.source_ref.domain != "local_economy" ||
        source.source_ref.kind != "facility_construction_started" ||
        source.source_ref.id.empty())
      fail("ConstructionOriginalAdoptionSourceRequired", source.source_ref.id);
    take(validate_facility_construction_catalog(
        context.catalog, context.identity, context.common_catalog,
        context.assets, context.facilities));
    if (context.identity.content_digest !=
        context.common.state().metadata.content_digest)
      fail("ConstructionAdoptionCatalogMismatch", source.source_ref.id);
    const FacilityConstruction *construction = nullptr;
    for (const auto &candidate : context.state.constructions)
      if (candidate.construction_id == source.source_ref.id) {
        if (construction)
          fail("DuplicateFacilityConstruction", source.source_ref.id);
        construction = &candidate;
      }
    if (!construction ||
        construction->construction_id != source.source_ref.id ||
        !context.common.actor(construction->builder_actor_id) ||
        construction->execution_binding.action_id !=
            construction->construction_id ||
        construction->execution_binding.action_kind !=
            "facility_construction" ||
        !same_source_ref(construction->execution_binding.source_owner_ref,
                         source.source_ref) ||
        construction->accepted_condition_head.actor_id !=
            construction->builder_actor_id ||
        !valid_source_ref(construction->accepted_presence_source.source_ref) ||
        construction->accepted_presence_source.source_schema <= 0 ||
        construction->authority_sources.empty() ||
        compare(construction->started_at,
                take(context.clock.read_current_moment())) > 0)
      fail("ConstructionOriginalAdoptionMismatch", source.source_ref.id);
    take(validate_work_binding(
        WorkExecutionBinding{construction->execution_binding}));
    for (std::size_t i = 0; i < construction->authority_sources.size(); ++i) {
      const auto &authority = construction->authority_sources[i];
      if (!valid_source_ref(authority.source_ref) ||
          authority.source_schema <= 0)
        fail("ConstructionOriginalAuthoritySourceRequired",
             construction->construction_id);
      for (std::size_t prior = 0; prior < i; ++prior)
        if (same_source_ref(construction->authority_sources[prior].source_ref,
                            authority.source_ref))
          fail("ConstructionDuplicateAuthoritySource", authority.source_ref.id);
    }
    const auto start_order = take(
        read_actual_common_fact_position(context.common, source.source_ref));
    const auto *terms = take(read_facility_construction_terms(
        context.catalog, construction->terms_ref));
    original_cost(context, *construction, *terms, start_order);
    if (const auto *action =
            context.common.action(construction->construction_id))
      if (action->id != construction->execution_binding.action_id ||
          action->kind != construction->execution_binding.action_kind ||
          action->actor_id != construction->builder_actor_id ||
          action->participants.size() != 1 ||
          action->participants.front() != construction->builder_actor_id ||
          action->start_tick != construction->started_at.half_day_tick)
        fail("ConstructionOriginalActionMismatch",
             construction->construction_id);
    return {true, construction, {}};
  } catch (const Failure &failure) {
    return {false, nullptr, failure.error};
  }
}
Outcome<const FacilityConstruction *>
read_facility_construction_by_id_for_original_dependency(
    const FacilityConstructionAdoptionContext &context, const StableId &id) {


  auto memory = CheckpointMemoryCharge(0);
  const MomentOwnerSource source{facility_construction_started_source(id), 1};
  return read_facility_construction_adoption_for_original_dependency(context,
                                                                     source);
}
} // namespace hanmo::resident
