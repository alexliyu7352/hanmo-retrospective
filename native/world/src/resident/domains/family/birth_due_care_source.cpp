// 反读出生时的原照护处置；Need 不代接管，当前 caregiver 不代历史满意。
#include "birth_due_care_source.hpp"
#include <algorithm>

namespace hanmo::resident {
namespace {

// 错误沿实际来源传播；不把缺 owning chain 化作正常无人接单。
struct CareSourceFailure {
  RuleError error;
};

// 技术性缺源保持明确拒绝，调用方撤回整个出生候选。
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw CareSourceFailure{common_failure(code, id, kind).error};
}

// 所有来源都使用原 typed Outcome，不缓存或猜测下游成功。
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw CareSourceFailure{std::move(result.error)};
  return std::move(result.value);
}

// Status 与 typed 来源采用相同错误边界。
void check(Status result) {
  if (!result.ok)
    throw CareSourceFailure{std::move(result.error)};
}

// 时刻比较保持完整有理精度，不从当前半日补写出生历史。
bool same(const WorldMoment &first, const WorldMoment &second) {
  return take(compare_world_moments(first, second)) == 0;
}

// 只借真实已验证目标与唯一 RequirementCreated，不存第二份目标状态。
struct OriginalBirthCare {
  BornCharacterSourceView birth;
  const CareRequirement *requirement = nullptr;
  const CareTemporalEvent *created = nullptr;
};

// 全链读取能处理后来的合法接管/失效；出生因果仍固定在原首次转换。
OriginalBirthCare original(const BirthDueCareSourceContext &sources,
                           const BirthFactRef &birth_ref) {
  const auto &care = sources.care;
  if (!care.births.bound_to(care.common) || !care.rules.bound_to(care.common))
    fail("BirthCareSameCandidateSourcesRequired", birth_ref.birth_fact_id,
         ErrorKind::unsupported_capability);
  OriginalBirthCare result;
  result.birth = take(care.births.read_original_birth(birth_ref));
  result.requirement = take(read_birth_care_requirement(care, birth_ref));
  if (!result.birth.birth || !result.birth.original_fact ||
      !result.requirement ||
      result.requirement->subject_actor_id != result.birth.birth->actor_id)
    fail("BirthCareOriginalRequirementRequired", birth_ref.birth_fact_id);
  for (const auto &entry : care.goals.transitions.rows()) {
    const auto &transition = entry.second;
    if (transition.requirement_id != result.requirement->id ||
        transition.kind != CareRequirementTransition::Kind::requirement_created)
      continue;
    if (result.created)
      fail("BirthCareCreatedTransitionNotUnique", result.requirement->id);
    result.created = take(read_care_goal_transition_source(
        care, {"care_requirements", "care_requirement_changed", entry.first}));
  }
  const auto *event =
      result.created
          ? std::get_if<CareRequirementCreatedEvent>(&result.created->body)
          : nullptr;
  const auto *origin =
      event ? std::get_if<BirthDueCareOrigin>(&event->origin) : nullptr;
  if (!event || !origin || event->source_episode != 1 ||
      event->requirement_id != result.requirement->id ||
      origin->birth_ref.birth_fact_id != birth_ref.birth_fact_id ||
      !same(result.created->occurred_at, result.birth.occurred_at))
    fail("BirthCareActualCreatedSourceMismatch", birth_ref.birth_fact_id);
  return result;
}

// S0 自己核全部 Need/claim/Task 原来源；Family 不能另造一个状态目标投影。
const StateNeed &initial_need(const BirthDueCareSourceContext &sources,
                              const OriginalBirthCare &birth) {
  const StateGoalRef goal = CareCoverageGoal{
      birth.requirement->id, birth.requirement->subject_actor_id};
  const auto *need = take(read_state_need_for_source_episode(
      sources.care.common, sources.affairs, goal, 1, sources.goals,
      sources.tasks, take(sources.care.clock.read_current_moment()),
      sources.policies));
  if (!need || need->source_episode != 1 || need->episode != 1 ||
      need->previous_episode_ref ||
      need->opened_tick != birth.birth.occurred_at.half_day_tick)
    fail("BirthCareOriginalNeedMismatch", birth.requirement->id);
  return *need;
}

// 只认实际满意事实；普通 AcceptedCaregiver、父母或原生育条款都不等于覆盖。
void accepted(const BirthDueCareSourceContext &sources,
              const OriginalBirthCare &birth, const SourceRef &assignment) {
  const auto *coverage = sources.care.coverage;
  if (!coverage || &coverage->handovers.acceptance.goals != &sources.care)
    fail("BirthCareActualCoverageSourcesRequired", assignment.id,
         ErrorKind::unsupported_capability);
  const auto *satisfied =
      take(read_care_coverage_satisfaction(*coverage, assignment));
  if (!satisfied || satisfied->requirement_id != birth.requirement->id ||
      satisfied->source_episode != 1)
    fail("BirthCareOriginalAssignmentMismatch", assignment.id);
  const auto actual =
      take(read_care_binding_coverage_v2(*coverage, satisfied->binding_ref));
  if (!actual.binding || !actual.accepted || !actual.handover_completed ||
      actual.satisfaction != satisfied ||
      actual.binding->child_actor_id != birth.birth.birth->actor_id ||
      actual.accepted->subject_actor_id != birth.birth.birth->actor_id ||
      actual.accepted->requirement_id != birth.requirement->id ||
      !same(actual.handover_completed->occurred_at, birth.birth.occurred_at))
    fail("BirthCareActualHandoverRequired", assignment.id);
  if (actual.ended_by &&
      take(compare_world_moments(actual.ended_by->occurred_at,
                                 birth.birth.occurred_at)) <= 0)
    fail("BirthCareCoverageAlreadyEndedAtBirth", assignment.id);
}

} // namespace

Status
validate_birth_due_care_disposition(const BirthDueCareSourceContext &sources,
                                    const BirthFactRef &birth_ref,
                                    const BirthCareDisposition &disposition) {
  try {
    const auto birth = original(sources, birth_ref);
    if (disposition.kind == BirthCareDisposition::Kind::care_need_opened) {
      if (!disposition.care_assignment_ref.domain.empty() ||
          !disposition.care_assignment_ref.kind.empty() ||
          !disposition.care_assignment_ref.id.empty() ||
          initial_need(sources, birth).need_id != disposition.care_need_ref)
        fail("BirthCareNeedDispositionMismatch", birth_ref.birth_fact_id);
    } else if (disposition.kind == BirthCareDisposition::Kind::accepted_care) {
      if (!disposition.care_need_ref.empty())
        fail("BirthCareUnexpectedNeedReference", birth_ref.birth_fact_id);
      accepted(sources, birth, disposition.care_assignment_ref);
    } else {
      fail("InvalidBirthCareDisposition", birth_ref.birth_fact_id);
    }
    return common_success();
  } catch (CareSourceFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

Outcome<BirthCareDisposition>
read_birth_due_original_care_need(const BirthDueCareSourceContext &sources,
                                  const BirthFactRef &birth_ref) {
  try {
    const auto birth = original(sources, birth_ref);
    BirthCareDisposition disposition;
    disposition.kind = BirthCareDisposition::Kind::care_need_opened;
    disposition.care_need_ref = initial_need(sources, birth).need_id;
    check(validate_birth_due_care_disposition(sources, birth_ref, disposition));
    return {true, std::move(disposition), {}};
  } catch (CareSourceFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

} // namespace hanmo::resident
