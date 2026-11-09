// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 施工期间与单阶段的真实服务覆盖反读；不把这部分证据冒成已授权容量应用。
#include "facility_construction_result_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
  return std::move(result.value);
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
// 入口先固定专业 kind/schema，Common 同名头单独存在不能充作结果。
void source(const MomentOwnerSource &value, const char *kind) {
  if (value.source_schema != 1 || value.source_ref.domain != "local_economy" ||
      value.source_ref.kind != kind || value.source_ref.id.empty())
    fail("ConstructionOriginalResultSourceRequired", value.source_ref.id);
}
std::size_t order(const FacilityConstructionServiceContext &context,
                  const SourceRef &ref) {
  return take(read_actual_common_fact_position(context.adoption.common, ref));
}
const ConstructionPeriodResult &
period(const FacilityConstructionServiceContext &context,
       const MomentOwnerSource &ref) {
  source(ref, "facility_construction_period");
  const ConstructionPeriodResult *result = nullptr;
  for (const auto &row : context.adoption.state.periods)
    if (row.receipt_id == ref.source_ref.id) {
      if (result)
        fail("DuplicateConstructionPeriod", ref.source_ref.id);
      result = &row;
    }
  if (!result)
    fail("ConstructionOriginalPeriodMissing", ref.source_ref.id);
  const auto actual = take(read_facility_construction_provided_worker_period(
      context, result->construction_id, result->actual_period.period_ordinal));
  if (!take(same_work_period_ref2(result->actual_period, actual.ref,
                                  context.spatial.budget)) ||
      compare(result->occurred_at, actual.ref.until))
    fail("ConstructionOriginalPeriodMismatch", result->receipt_id);
  const auto start_order = order(
      context, facility_construction_started_source(result->construction_id));
  if (order(context, ref.source_ref) <= start_order)
    fail("ConstructionOriginalPeriodOrderMismatch", result->receipt_id);
  for (const auto &other : context.adoption.state.periods)
    if (&other != result && other.construction_id == result->construction_id &&
        other.actual_period.period_ordinal ==
            result->actual_period.period_ordinal)
      fail("ConstructionPeriodOrdinalReused", result->receipt_id);
  return *result;
}
} // namespace

Outcome<const ConstructionPeriodResult *>
read_facility_construction_period_service_evidence(
    const FacilityConstructionServiceContext &context,
    const MomentOwnerSource &ref) {
  try {
    return {true, &period(context, ref), {}};
  } catch (const Failure &failure) {
    return {false, nullptr, failure.error};
  }
}

Outcome<const ConstructionStageResult *>
read_facility_construction_stage_service_evidence(
    const FacilityConstructionServiceContext &context,
    const MomentOwnerSource &ref) {
  try {
    source(ref, "facility_construction_stage");
    const ConstructionStageResult *result = nullptr;
    for (const auto &row : context.adoption.state.stages)
      if (row.result_id == ref.source_ref.id) {
        if (result)
          fail("DuplicateConstructionStage", ref.source_ref.id);
        result = &row;
      }
    if (!result)
      fail("ConstructionOriginalStageMissing", ref.source_ref.id);
    const auto *construction =
        take(read_facility_construction_adoption_for_original_dependency(
            context.adoption,
            {facility_construction_started_source(result->construction_id),
             1}));
    const auto *terms = take(read_facility_construction_terms(
        context.adoption.catalog, construction->terms_ref));
    if (result->stage_ordinal != 0 ||
        result->actual_cost_ref != construction->actual_cost_ref ||
        terms->duration_halfdays < 1 ||
        result->actual_period_refs.size() !=
            static_cast<std::uint64_t>(terms->duration_halfdays))
      fail("ConstructionOriginalStageCoverageMismatch", result->result_id);
    for (const auto &other : context.adoption.state.stages)
      if (&other != result && other.construction_id == result->construction_id)
        fail("ConstructionStageReused", result->result_id);
    const auto stage_order = order(context, ref.source_ref);
    std::size_t previous_order = order(
        context, facility_construction_started_source(result->construction_id));
    const ConstructionPeriodResult *previous = nullptr;
    for (std::size_t i = 0; i < result->actual_period_refs.size(); ++i) {
      const auto period_ref =
          facility_construction_period_source(result->actual_period_refs[i]);
      const auto &provided = period(context, {period_ref, 1});
      const auto period_order = order(context, period_ref);
      if (provided.construction_id != result->construction_id ||
          provided.actual_period.period_ordinal != static_cast<Integer>(i) ||
          period_order <= previous_order || period_order >= stage_order ||
          (previous && compare(previous->actual_period.until,
                               provided.actual_period.from) > 0))
        fail("ConstructionOriginalStagePeriodMismatch", result->result_id);
      previous = &provided;
      previous_order = period_order;
    }
    if (!previous || compare(result->occurred_at, previous->occurred_at))
      fail("ConstructionOriginalStageMomentMismatch", result->result_id);
    return {true, result, {}};
  } catch (const Failure &failure) {
    return {false, nullptr, failure.error};
  }
}
} // namespace hanmo::resident
