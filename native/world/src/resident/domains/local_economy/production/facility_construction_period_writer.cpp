// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 真实已供期/原用途窗/阶段结果同候选连接，不存进度、不补时间、不提前增容量。
#include "facility_construction_period_writer.hpp"
#include "facility_construction_result_read.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <limits>

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
StableId fresh(const StableId &value) {
  return checkpoint_fresh_string_copy(value);
}
// 原 tracked 根及普通字段预留必须全属同候选、同 job；没有旧 clone append 信用。
void candidate(const FacilityConstructionPeriodWriteContext &x) {
  const auto &a = x.service.adoption;
  const auto memory = current_checkpoint_memory();
  const auto &fields = x.owned_fields.resource();
  if (&x.common != &a.common || &x.state != &a.state ||
      &x.elapsed != &x.service.elapsed ||
      &x.common != &x.assets.installation.common || !memory || !fields ||
      !memory->shares_job(*fields))
    fail("ConstructionActualPeriodCandidateRequired");
  for (const auto &resource : {x.state.constructions.get_allocator().resource(),
                               x.state.start_costs.get_allocator().resource(),
                               x.state.periods.get_allocator().resource(),
                               x.state.stages.get_allocator().resource()})
    if (!resource || !memory->shares_job(*resource))
      fail("ConstructionActualPeriodRootOwnerRequired");
}
// 只收实际 S0 /2 的窗口；原 consent 和当前全产权历史独立反读。
// 初始用途第一次真实终止后不复活，等于 until 仅覆盖此前半开服务窗。
void usage(const FacilityConstructionPeriodWriteContext &x,
           const FacilityConstruction &construction,
           const WorkPeriodRef2 &period) {
  take(validate_production_asset_construction_period_usage(
      x.assets, x.service.adoption, construction, period));
}
// 新精确容量壳只接逐条 typed move；失败由外层丢整个 World 候选。
template <class T> void append(CheckpointVector<T> &rows, T &&row) {
  if (rows.size() == rows.max_size())
    fail("ConstructionResultCapacityExceeded");
  CheckpointVector<T> next(rows.get_allocator());
  next.reserve(rows.size() + 1);
  for (auto &old : rows)
    next.emplace_back(std::move(old));
  next.emplace_back(std::move(row));
  rows.swap(next);
}
// 只为一次真正持久复制收费；numeric Moment 继续由原 exact allocator 拥有。

ActorActionWorkBinding copy_binding(const ActorActionWorkBinding &value) {
  return {fresh(value.action_id),
          fresh(value.action_kind),
          {fresh(value.source_owner_ref.domain),
           fresh(value.source_owner_ref.kind),
           fresh(value.source_owner_ref.id)}};
}
const ConstructionPeriodResult &
read_period(const FacilityConstructionPeriodWriteContext &x,
            const StableId &id) {


  auto memory = CheckpointMemoryCharge(0);
  return *take(read_facility_construction_period_service_evidence(
      x.service, {facility_construction_period_source(id), 1}));
}
const ConstructionStageResult &
read_stage(const FacilityConstructionPeriodWriteContext &x,
           const StableId &id) {


  auto memory = CheckpointMemoryCharge(0);
  return *take(read_facility_construction_stage_service_evidence(
      x.service, {facility_construction_stage_source(id), 1}));
}
} // namespace
Outcome<PendingFacilityConstructionResult>
record_facility_construction_period_in_candidate(
    FacilityConstructionPeriodWriteContext &x, const StableId &id,
    Integer ordinal) {
  try {
    candidate(x);
    const auto &construction =
        *take(read_facility_construction_by_id_for_original_dependency(
            x.service.adoption, id));
    const auto &terms = *take(read_facility_construction_terms(
        x.service.adoption.catalog, construction.terms_ref));
    if (ordinal < 0 || ordinal >= terms.duration_halfdays)
      fail("ConstructionActualPeriodOrdinalRequired", id);
    std::size_t retained_count = 0;
    std::optional<std::size_t> replay;
    bool future_period = false;
    for (std::size_t i = 0; i < x.state.periods.size(); ++i) {
      const auto &saved = x.state.periods[i];
      if (saved.construction_id != id)
        continue;
      const auto &actual = read_period(x, saved.receipt_id);
      usage(x, construction, actual.actual_period);
      if (actual.actual_period.period_ordinal == ordinal)
        replay = i;
      future_period |= actual.actual_period.period_ordinal > ordinal;
      ++retained_count;
    }
    if (replay)
      return {true, {*replay, true}, {}};
    if (future_period || retained_count != static_cast<std::uint64_t>(ordinal))
      fail("ConstructionActualPeriodPredecessorRequired", id);
    for (const auto &stage : x.state.stages)
      if (stage.construction_id == id)
        fail("ConstructionAlreadyCompleted", id);
    auto provided = take(read_facility_construction_provided_worker_period(
        x.service, id, ordinal));
    const auto now = take(x.service.adoption.clock.read_current_moment());
    if (compare(now, provided.ref.until))
      fail("ConstructionActualPeriodBoundaryRequired", id);
    usage(x, construction, provided.ref);
    auto result_id =
        take(x.common.next_owned_id("facility_construction_period"));


    (x.owned_fields).grow(0);
    ConstructionPeriodResult result{
        fresh(result_id.id()),
        fresh(id),
        {WorkExecutionBinding{copy_binding(construction.execution_binding)},
         ordinal, provided.ref.from, provided.ref.until, 2},
        now};
    const auto index = x.state.periods.size();
    append(x.state.periods, std::move(result));
    take(x.common.append_fact_borrowed(
        result_id.id(), "facility_construction_period", "local_economy"));
    read_period(x, result_id.id());
    return {true, {index, false}, {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<PendingFacilityConstructionResult>
complete_facility_construction_stage_in_candidate(
    FacilityConstructionPeriodWriteContext &x, const StableId &id) {
  try {
    candidate(x);
    const auto &construction =
        *take(read_facility_construction_by_id_for_original_dependency(
            x.service.adoption, id));
    for (std::size_t i = 0; i < x.state.stages.size(); ++i)
      if (x.state.stages[i].construction_id == id) {
        read_stage(x, x.state.stages[i].result_id);
        const auto *done = find_fixed_elapsed(x.elapsed, id);
        if (!done || !std::holds_alternative<ElapsedTerminal>(done->timing) ||
            x.common.action(id) ||
            x.common.actor(construction.builder_actor_id)->action_id == id)
          fail("ConstructionActualTerminalReplayRequired", id);
        return {true, {i, true}, {}};
      }
    const auto &terms = *take(read_facility_construction_terms(
        x.service.adoption.catalog, construction.terms_ref));
    std::size_t actual_count = 0;
    for (const auto &row : x.state.periods)
      if (row.construction_id == id) {
        ++actual_count;
        if (row.actual_period.period_ordinal < 0 ||
            row.actual_period.period_ordinal >= terms.duration_halfdays)
          fail("ConstructionActualPeriodOrdinalRequired", id);
      }
    if (actual_count != static_cast<std::uint64_t>(terms.duration_halfdays))
      fail("ConstructionActualStagePeriodsRequired", id);
    CheckpointVector<const ConstructionPeriodResult *> periods;
    periods.reserve(static_cast<std::size_t>(terms.duration_halfdays));
    for (Integer ordinal = 0; ordinal < terms.duration_halfdays; ++ordinal) {
      const ConstructionPeriodResult *selected = nullptr;
      for (const auto &row : x.state.periods)
        if (row.construction_id == id &&
            row.actual_period.period_ordinal == ordinal) {
          if (selected)
            fail("ConstructionPeriodOrdinalReused", id);
          selected = &read_period(x, row.receipt_id);
        }
      if (!selected)
        fail("ConstructionActualStagePeriodsRequired", id);
      usage(x, construction, selected->actual_period);
      periods.emplace_back(selected);
    }
    const auto now = take(x.service.adoption.clock.read_current_moment());
    const auto *elapsed = find_fixed_elapsed(x.elapsed, id);
    if (periods.empty() || compare(now, periods.back()->occurred_at) ||
        !elapsed || !std::holds_alternative<ElapsedRunning>(elapsed->timing) ||
        compare(now, elapsed->last_settled_at) ||
        take(compare_fractions(elapsed->credited_units.value,
                               elapsed->required_units.value,
                               x.service.spatial.budget)))
      fail("ConstructionActualStageCompletionBoundaryRequired", id);
    const FixedServiceTransition *previous = nullptr;
    for (const auto &row : x.elapsed.fixed_service_transitions)
      if (row.action_ref == id &&
          (!previous || row.transition_ordinal > previous->transition_ordinal))
        previous = &row;
    if (!previous ||
        previous->transition_ordinal == std::numeric_limits<Integer>::max())
      fail("ConstructionActualStageTransitionRequired", id);
    auto result_id =
        take(x.common.next_owned_id("facility_construction_stage"));


    (x.owned_fields).grow(0);
    ConstructionStageResult result{fresh(result_id.id()),
                                   fresh(id),
                                   0,
                                   now,
                                   {},
                                   fresh(construction.actual_cost_ref)};
    result.actual_period_refs.reserve(periods.size());
    for (const auto *period : periods)
      result.actual_period_refs.emplace_back(fresh(period->receipt_id));
    const auto index = x.state.stages.size();
    append(x.state.stages, std::move(result));
    take(x.common.append_fact_borrowed(
        result_id.id(), "facility_construction_stage", "local_economy"));
    read_stage(x, result_id.id());
    const SourceRefBuildView execution{"local_economy",
                                       "facility_construction_started", id};
    const SourceRefBuildView terminal{
        "local_economy", "facility_construction_stage", result_id.id()};
    const std::vector<FixedServiceInputRef> no_inputs;
    FixedServiceTransitionInput transition{
        FixedServiceTerminalTransitionBuildView{
            result_id.id(), 1, id, previous->transition_ordinal + 1,
            previous->transition_id, FixedTransitionKind::terminal, now,
            elapsed->credited_units, execution, 1,
            FixedOwnerTerminatedBuildView{terminal, 1}, no_inputs,
            previous->input_basis_refs, FixedTerminalDisposition::completed}};
    const auto &registration = facility_construction_action_time_registration();
    auto prepared = take(prepare_fixed_service_change(
        x.elapsed, id, transition.row(), std::nullopt, registration,
        x.service.scale, x.action_sources, x.service.spatial.budget));
    take(apply_fixed_service_change_in_candidate(
        x.elapsed, std::move(prepared), registration, x.service.scale,
        x.action_sources, x.service.spatial.budget));


    auto memory = CheckpointMemoryCharge(0);
    ActionMutation erase;
    erase.kind = ActionMutation::Kind::erase;
    erase.action.id = fresh(id);
    take(x.common.apply_action(erase));
    ActorMutation free;
    free.actor_id = fresh(construction.builder_actor_id);
    free.action_id = StableId{};
    take(x.common.apply_actor(free));
    return {true, {index, false}, {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
