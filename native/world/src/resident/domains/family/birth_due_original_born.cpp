// 只读实际正向来源前缀；没有供养/照护/船载成功回退，也不成为完整出生 provider。
#include "birth_due_original_born.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"

namespace hanmo::resident {
namespace {

// 跨域错误保持原始原因，外层撤回完整候选，不做补偿型第二次出生。
struct BornSourceFailure {
  RuleError error;
};

// 缺真实条件分支是能力错误，不返回全成功或零载荷结果。
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw BornSourceFailure{common_failure(code, id, kind).error};
}

// 下游专业来源逐个重读，结果引用只在本次同步调用内有效。
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw BornSourceFailure{std::move(result.error)};
  return std::move(result.value);
}

// 不以异常吞掉原专业 validator 的拒绝。
void check(Status result) {
  if (!result.ok)
    throw BornSourceFailure{std::move(result.error)};
}

// SourceRef 三元身份和真实时刻分别比较，绝不只比 ID。
bool same(const SourceRef &first, const SourceRef &second) {
  return first.domain == second.domain && first.kind == second.kind &&
         first.id == second.id;
}

// 全精度比较保留同候选原生时刻，不截成 floor tick。
bool same(const WorldMoment &first, const WorldMoment &second) {
  return take(compare_world_moments(first, second)) == 0;
}

// 预定源完全重读原接受；不把调用方带来的 due/policy 当作权威。
void schedule(const ScheduledBirthSource &actual,
              const ScheduledBirthSource &requested) {
  if (actual.arrangement_id != requested.arrangement_id ||
      actual.terms_version != requested.terms_version ||
      actual.due_tick != requested.due_tick ||
      !same(actual.occurrence, requested.occurrence) ||
      !same(actual.source_ref, requested.source_ref) ||
      !actor_foundation_rule_equal(actual.selected_policy_ref,
                                   requested.selected_policy_ref))
    fail("BirthDueOriginalScheduleMismatch", requested.arrangement_id);
}

} // namespace

// 原同世界指针和当前时刻都必须一致，不用空表或零 revision 掩盖绑定缺失。
Status validate_birth_due_original_born_bindings(
    const BirthDueOriginalBornContext &sources) {
  try {
    const auto &births = sources.births;
    if (&sources.population.common != &births.common ||
        &sources.population.births != &births || !births.catalog.enabled ||
        births.catalog.selected_version != 4 ||
        births.access.foundation != &sources.foundation ||
        births.access.foundation_catalog != &sources.foundation_catalog ||
        births.access.placement != &sources.placement ||
        !births.access.people ||
        !births.access.people->bound_to(births.common) ||
        !sources.placement.bound_to(births.common))
      fail("BirthDueOriginalBornBindingsRequired", {},
           ErrorKind::unsupported_capability);
    check(validate_born_character_source_structure(births));
    check(validate_born_population_source_bindings(sources.population));
    if (!same(take(births.access.people->read_current_moment()),
              take(sources.population.clock.read_current_moment())))
      fail("BirthDueOriginalPopulationClockMismatch");
    return common_success();
  } catch (BornSourceFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

// 接受、出生、S1 和人口各由原 reader 先独立核验，不递归依赖完整 Due 结果。
Outcome<BirthDueOriginalBornView>
read_birth_due_original_born_prefix(const BirthDueOriginalBornContext &context,
                                    const ScheduledBirthSource &requested,
                                    const BornBirthDueResult &result) {
  try {
    check(validate_birth_due_original_born_bindings(context));
    const auto &sources = context.births;
    if (result.birth_fact_ref.domain != "family_birth" ||
        result.birth_fact_ref.kind != "family_birth_recorded")
      fail("BirthDueOriginalBirthSourceRequired", result.birth_fact_ref.id);
    const BirthFactRef ref{result.birth_fact_ref.id};
    auto birth = take(read_born_character_source(sources, ref));
    const auto actual_schedule = take(read_scheduled_birth_source(
        sources.common, sources.interactions, sources.family_catalog,
        sources.catalog, sources.births, sources.access,
        requested.arrangement_id));
    schedule(actual_schedule, requested);
    if (!birth.birth || !birth.accepted || !birth.accepted->confirmation ||
        !birth.original_fact ||
        birth.birth->actor_id != result.child_actor_id ||
        birth.birth->accepted_id != requested.arrangement_id ||
        !same(birth.occurred_at, requested.occurrence) ||
        !birth.birth->location_result_ref ||
        !same(birth.birth->location_result_ref->source_ref,
              result.location_result_ref))
      fail("BirthDueActualChildMismatch", result.birth_fact_ref.id);
    check(validate_born_character_identity_and_placement(
        sources, context.foundation, context.foundation_catalog,
        context.placement, ref));
    const MomentOwnerSource birth_source{result.birth_fact_ref, 4};
    auto placement = take(context.placement.read_birth_placement_application(
        birth_source, *birth.birth->location_result_ref));
    if (!placement.actual_placement ||
        placement.child_actor_id != result.child_actor_id ||
        placement.birth_arrangement_id != requested.arrangement_id ||
        placement.delivery_actor_id !=
            birth.accepted->confirmation->terms.gestating_actor_id ||
        !same(placement.occurred_at, birth.occurred_at) ||
        placement.transport_binding_ref != result.transport_binding_ref ||
        !same(placement.actual_birth_source.source_ref,
              result.birth_fact_ref) ||
        placement.actual_birth_source.source_schema != 4 ||
        !same(placement.location_result.source_ref,
              result.location_result_ref) ||
        placement.location_result.source_schema !=
            birth.birth->location_result_ref->source_schema)
      fail("BirthDueActualPlacementMismatch", result.birth_fact_ref.id);
    const auto *population = take(read_named_birth_population_transition(
        context.population, result.population_transition_ref, 1));
    if (!population ||
        !same(population->birth_fact_ref, result.birth_fact_ref) ||
        population->child_actor_id != result.child_actor_id ||
        !same(population->occurred_at, birth.occurred_at))
      fail("BirthDueActualPopulationMismatch", result.birth_fact_ref.id);
    const bool fleet =
        placement.actual_placement->placement_kind == BirthPlacementKind::fleet;
    if (!fleet && result.carrier_load_result_ref)
      fail("BirthDueUnexpectedPassengerLoadSource", result.birth_fact_ref.id);
    // fleet 的真实载荷和所有分支的供养/照护在完整组合 reader 另验；此处不代答。
    return {
        true, {std::move(birth), std::move(placement), population, fleet}, {}};
  } catch (BornSourceFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
