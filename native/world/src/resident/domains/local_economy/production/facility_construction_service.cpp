// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原工人的接受与实际服务来源；信用和暂停仍由唯一 S0 历史提供。
#include "facility_construction_service_original.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
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
// 仅枚举本次实际复制的五行输入；不按序列化大小估算，也不多留临时副本。
CheckpointMemoryCharge input_memory(const MomentOwnerSourceBuildView &start,
                                    const MomentOwnerSourceBuildView &body,
                                    const MomentOwnerSourceBuildView &presence,
                                    const CatalogIdentity &identity,
                                    std::string_view purpose,
                                    std::string_view rule) {


  return CheckpointMemoryCharge(0);
}
// 调用方已预留本次真实字段复制；只构造原三字段源与 schema，不生成事实。
FixedAppliedInput copy_input(const MomentOwnerSourceBuildView &value) {
  return {{StableId(value.source_ref.domain), StableId(value.source_ref.kind),
           StableId(value.source_ref.id)},
          value.source_schema};
}
WorldMoment context(const FacilityConstructionServiceContext &x) {
  const auto now = take(x.adoption.clock.read_current_moment());
  if (&x.adoption.common != &x.body.common ||
      &x.adoption.common != &x.spatial.common ||
      &x.adoption.common_catalog != &x.spatial.common_catalog ||
      &x.body.state != &x.spatial.foundation ||
      &x.body.catalog != &x.spatial.actors || x.body.elapsed != &x.elapsed ||
      x.body.scale != &x.scale || compare(now, x.body.actual_now) ||
      compare(now, take(x.spatial.spatial.current_moment())) ||
      !same_original_catalog_identity(x.adoption.identity,
                                      x.body.catalog.identity))
    fail("ConstructionServiceContextMismatch");
  return now;
}
// 借真实已登记的 purpose，避免每次查询先复制一份 catalog digest；未登记
// 或错 reducer 仍拒绝，不把一个 caller 拼出的同名规则当成注册证明。
const ActorFoundationRuleRef &
purpose(const FacilityConstructionServiceContext &x) {
  const CharacterPurposeBinding *found = nullptr;
  for (const auto &row : x.body.catalog.capability_purpose_bindings.rows())
    if (row.first == "facility_construction_worker") {
      if (found)
        fail("ConstructionDuplicateWorkerPurpose", row.first);
      found = &row.second;
    }
  if (!found || found->purpose_ref.id != "facility_construction_worker" ||
      found->purpose_ref.version != 1 ||
      found->purpose_ref.catalog_digest !=
          x.body.catalog.identity.content_digest ||
      found->reducer != CharacterPurposeBinding::Reducer::normal_work ||
      !x.body.catalog.normal_work_efficiency_rule ||
      !actor_foundation_rule_equal(
          found->rule_ref,
          x.body.catalog.normal_work_efficiency_rule->rule_ref))
    fail("ConstructionOriginalWorkerPurposeRequired");
  return found->purpose_ref;
}
const ConstructionTerms &terms(const FacilityConstructionServiceContext &x,
                               const FacilityConstruction &construction) {
  return *take(read_facility_construction_terms(x.adoption.catalog,
                                                construction.terms_ref));
}
void rule(const FacilityConstructionServiceContext &x,
          const CharacterCapabilityView &capability, const StableId &id) {
  if (!actor_foundation_rule_equal(capability.purpose_ref, purpose(x)) ||
      !x.body.catalog.normal_work_efficiency_rule ||
      !actor_foundation_rule_equal(
          capability.rule_ref,
          x.body.catalog.normal_work_efficiency_rule->rule_ref))
    fail("ConstructionOriginalCapabilityRuleRequired", id);
}
void at_site(const FacilityConstructionServiceContext &x,
             const ConstructionTerms &terms, const MovementAnchor &anchor) {
  if (!take(same_spatial_anchor(
          anchor,
          take(x.spatial.spatial.original_site_anchor(terms.place_id)))))
    fail("ConstructionActualWorkerPresenceRequired", terms.terms_ref.id);
}
// 冻结接受 head 必须在原开工点本就有资格并真实在场，不能等今日恢复后
// 回填。这里只核工人，原财产/支出权仍由建设完整准入独立验证。
void accepted_worker(const FacilityConstructionServiceContext &x,
                     const FacilityConstruction &construction) {
  const auto capability = take(read_character_capability_at_head(
      x.body, construction.accepted_condition_head, purpose(x), std::nullopt,
      construction.started_at));
  rule(x, capability, construction.construction_id);
  if (!capability.allowed || capability.efficiency_bp <= 0)
    fail("ConstructionAcceptedWorkerCapabilityRequired",
         construction.construction_id);
  take(read_character_condition_head_source(
      x.body, construction.accepted_condition_head, construction.started_at));
  const auto &presence = construction.accepted_presence_source;
  if (!is_actor_spatial_head_source_kind(presence.source_ref))
    fail("ConstructionAcceptedSpatialHeadRequired",
         construction.construction_id);
  const auto location = take(read_actor_spatial_location_from_head_pool(
      x.spatial, construction.builder_actor_id,
      {{presence.source_ref, presence.source_schema}}, construction.started_at,
      x.military_handoffs));
  at_site(x, terms(x, construction),
          *take(require_actor_ordinary_movement_anchor(location)));
}
// 五个输入分别指原接受、原身体、原位置以及锁内 purpose/reducer。
// 后两者只约束所选规则，不让 caller 自填效率或删减实际历史。
void provided_interval(const FacilityConstructionServiceContext &x,
                       const FacilityConstruction &construction,
                       const ConstructionTerms &accepted,
                       const ActualServiceInterval &interval) {
  if (interval.input_basis_refs.size() != 5 ||
      compare(interval.from, interval.until) >= 0)
    fail("ConstructionOriginalServiceInputCoverage",
         construction.construction_id);
  const auto *start =
      std::get_if<FixedAppliedInput>(&interval.input_basis_refs[0]);
  const auto *body =
      std::get_if<FixedAppliedInput>(&interval.input_basis_refs[1]);
  const auto *presence =
      std::get_if<FixedAppliedInput>(&interval.input_basis_refs[2]);
  const auto *selected_purpose =
      std::get_if<FixedLockedRuleInput>(&interval.input_basis_refs[3]);
  const auto *selected_rule =
      std::get_if<FixedLockedRuleInput>(&interval.input_basis_refs[4]);
  if (!start || start->source_schema != 1 ||
      !same_source_ref(start->source_ref, facility_construction_started_source(
                                              construction.construction_id)) ||
      !body || !presence || !selected_purpose || !selected_rule ||
      !is_actor_spatial_head_source_kind(presence->source_ref))
    fail("ConstructionOriginalServiceInputKinds", construction.construction_id);
  const auto original_body = take(read_character_condition_at_source(
      x.body, body->source_ref, body->source_schema, interval.from));
  if (original_body.head.actor_id != construction.builder_actor_id)
    fail("ConstructionWorkerBodyActorMismatch", construction.construction_id);
  const auto capability = take(read_character_capability_for_span(
      x.body, original_body.head, purpose(x), std::nullopt,
      {interval.from, interval.until}));
  rule(x, capability, construction.construction_id);
  if (!capability.allowed || capability.efficiency_bp <= 0 ||
      selected_purpose->rule_ref != capability.purpose_ref.id ||
      selected_rule->rule_ref != capability.rule_ref.id ||
      !same_original_catalog_identity(selected_purpose->catalog_identity,
                                      x.body.catalog.identity) ||
      !same_original_catalog_identity(selected_rule->catalog_identity,
                                      x.body.catalog.identity))
    fail("ConstructionActualWorkerCapabilityRequired",
         construction.construction_id);
  const auto head = take(read_actor_spatial_location_from_head_pool(
      x.spatial, construction.builder_actor_id,
      {{presence->source_ref, presence->source_schema}}, interval.from,
      x.military_handoffs));
  at_site(x, accepted, *take(require_actor_ordinary_movement_anchor(head)));
  const auto &spatial = x.spatial;
  const auto located = take(read_actor_stationary_location_span(
      spatial.common, spatial.common_catalog, spatial.movement,
      spatial.carriers, spatial.registrations, spatial.foundation,
      spatial.actors, spatial.spatial, construction.builder_actor_id,
      presence->source_ref, {interval.from, interval.until},
      x.military_handoffs, spatial.budget, spatial.family_catalog,
      spatial.births, spatial.military_actions));
  at_site(x, accepted, located.actual_anchor);
}
} // namespace

FacilityConstructionServiceInputs::FacilityConstructionServiceInputs(
    const MomentOwnerSourceBuildView &start,
    const MomentOwnerSourceBuildView &body,
    const MomentOwnerSourceBuildView &presence, const CatalogIdentity &identity,
    std::string_view purpose, std::string_view rule)
    : copied_fields_(
          input_memory(start, body, presence, identity, purpose, rule)) {
  values_.reserve(5);
  values_.emplace_back(copy_input(start));
  values_.emplace_back(copy_input(body));
  values_.emplace_back(copy_input(presence));
  values_.emplace_back(FixedLockedRuleInput{identity, StableId(purpose)});
  values_.emplace_back(FixedLockedRuleInput{identity, StableId(rule)});
}

Status visit_facility_construction_adoptable_worker(
    const FacilityConstructionServiceContext &x, const ActivityRead &activity,
    const StableId &actor_id, const EconomicRuleRef &terms_ref,
    FacilityConstructionWorkerAdoptionSink &sink) {
  try {
    const auto now = context(x);
    take(validate_facility_construction_catalog(
        x.adoption.catalog, x.adoption.identity, x.adoption.common_catalog,
        x.adoption.assets, x.adoption.facilities));
    const auto *selected =
        take(read_facility_construction_terms(x.adoption.catalog, terms_ref));
    const auto *actor = x.adoption.common.actor(actor_id);
    if (!actor || !actor->alive || !actor->available ||
        !actor->action_id.empty() || activity.in_open_battle(actor_id) ||
        activity.in_interaction(actor_id))
      fail("ConstructionActualWorkerBusy", actor_id);
    const auto *head = take(read_verified_current_character_condition_head(
        x.body, x.adoption.common, x.body.state, actor_id, x.adoption.clock));
    const auto capability = take(read_character_capability_at_head(
        x.body, *head, purpose(x), std::nullopt, now));
    rule(x, capability, actor_id);
    if (!capability.allowed || capability.efficiency_bp <= 0)
      fail("ConstructionAcceptedWorkerCapabilityRequired", actor_id);
    const auto &spatial = x.spatial;
    const auto prefix = take(read_current_common_fact_prefix(spatial.common));


    auto prefix_memory = CheckpointMemoryCharge(0);
    CommonActualFactPrefix accepted_prefix{prefix.common_fact_count,
                                           std::nullopt};
    if (prefix.last_common_fact)
      accepted_prefix.last_common_fact = SourceRef{
          checkpoint_fresh_string_copy(prefix.last_common_fact->domain),
          checkpoint_fresh_string_copy(prefix.last_common_fact->kind),
          checkpoint_fresh_string_copy(prefix.last_common_fact->id)};
    const auto location =
        take(read_actor_spatial_location_at_actual_fact_prefix(
            spatial.common, spatial.common_catalog, spatial.movement,
            spatial.carriers, spatial.registrations, spatial.foundation,
            spatial.actors, spatial.spatial, actor_id, now, accepted_prefix,
            x.military_handoffs, spatial.budget, spatial.family_catalog,
            spatial.births, spatial.military_actions));
    if (location.actual_location_sources.size() != 1)
      fail("ConstructionOriginalSpatialHeadRequired", actor_id);
    at_site(x, *selected,
            *take(require_actor_ordinary_movement_anchor(location)));
    return sink.accept(*head, location.actual_location_sources.front(), now);
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<FacilityConstructionServiceInputs>
read_facility_construction_current_service_inputs(
    const FacilityConstructionServiceContext &x, const StableId &id) {
  try {
    const auto now = context(x);
    const auto &construction =
        *take(read_facility_construction_by_id_for_original_dependency(
            x.adoption, id));
    accepted_worker(x, construction);
    const auto *action = x.adoption.common.action(id);
    const auto *actor = x.adoption.common.actor(construction.builder_actor_id);
    if (!action || !actor || actor->action_id != id)
      fail("ConstructionActualExclusiveActionRequired", id);
    const auto body = take(read_current_character_condition(
        x.body, construction.builder_actor_id));
    const auto capability = take(read_character_capability_at_head(
        x.body, body.head, purpose(x), std::nullopt, now));
    rule(x, capability, id);
    const auto body_source =
        take(read_character_condition_head_source(x.body, body.head, now));
    const auto &spatial = x.spatial;
    const auto prefix = take(read_current_common_fact_prefix(spatial.common));
    CommonActualFactPrefix accepted_prefix{prefix.common_fact_count,
                                           std::nullopt};
    if (prefix.last_common_fact)
      accepted_prefix.last_common_fact =
          SourceRef{prefix.last_common_fact->domain,
                    prefix.last_common_fact->kind, prefix.last_common_fact->id};
    const auto location =
        take(read_actor_spatial_location_at_actual_fact_prefix(
            spatial.common, spatial.common_catalog, spatial.movement,
            spatial.carriers, spatial.registrations, spatial.foundation,
            spatial.actors, spatial.spatial, construction.builder_actor_id, now,
            accepted_prefix, x.military_handoffs, spatial.budget,
            spatial.family_catalog, spatial.births, spatial.military_actions));
    if (location.actual_location_sources.size() != 1)
      fail("ConstructionOriginalSpatialHeadRequired", id);
    at_site(x, terms(x, construction),
            *take(require_actor_ordinary_movement_anchor(location)));
    const auto &presence = location.actual_location_sources.front();
    return {
        true,
        FacilityConstructionServiceInputs{
            {{"local_economy", "facility_construction_started", id}, 1},
            {{body_source.source.domain, body_source.source.kind,
              body_source.source.id},
             body_source.source_schema},
            {{presence.source.domain, presence.source.kind, presence.source.id},
             presence.schema},
            x.body.catalog.identity,
            capability.purpose_ref.id,
            capability.rule_ref.id},
        {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<FixedServiceWorkPeriodRead2>
read_facility_construction_provided_worker_period(
    const FacilityConstructionServiceContext &x, const StableId &id,
    Integer ordinal) {
  try {
    const auto now = context(x);
    const auto &construction =
        *take(read_facility_construction_by_id_for_original_dependency(
            x.adoption, id));
    accepted_worker(x, construction);
    const auto &accepted = terms(x, construction);
    auto period = take(read_fixed_service_work_period2(
        x.elapsed, construction.execution_binding, ordinal,
        accepted.duration_halfdays, now, x.scale, x.spatial.budget));
    for (const auto &interval : period.actual_service.intervals)
      provided_interval(x, construction, accepted, interval);
    return {true, std::move(period), {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
// 已冻结接受与当前真实身体都走同一 purpose/reducer，未读 S0 信用。
Outcome<CharacterCapabilityView>
read_facility_construction_current_worker_capability(
    const FacilityConstructionServiceContext &x, const StableId &id) {
  try {
    const auto now = context(x);
    const auto &construction =
        *take(read_facility_construction_by_id_for_original_dependency(
            x.adoption, id));
    accepted_worker(x, construction);
    const auto body = take(read_current_character_condition(
        x.body, construction.builder_actor_id));
    auto capability = take(read_character_capability_at_head(
        x.body, body.head, purpose(x), std::nullopt, now));
    rule(x, capability, id);
    return {true, std::move(capability), {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<CharacterCapabilityView> read_facility_construction_transition_worker(
    const FacilityConstructionServiceContext &x, const StableId &id,
    const FixedServiceTransition &transition) {
  try {
    const auto now = context(x);
    const auto &construction =
        *take(read_facility_construction_by_id_for_original_dependency(
            x.adoption, id));
    accepted_worker(x, construction);
    if (transition.action_ref != id ||
        compare(transition.occurred_at, construction.started_at) < 0 ||
        compare(transition.occurred_at, now) > 0 ||
        transition.input_basis_refs.size() != 5)
      fail("ConstructionTransitionInputCoverageMismatch", id);
    const auto *start =
        std::get_if<FixedAppliedInput>(&transition.input_basis_refs[0]);
    const auto *body =
        std::get_if<FixedAppliedInput>(&transition.input_basis_refs[1]);
    const auto *presence =
        std::get_if<FixedAppliedInput>(&transition.input_basis_refs[2]);
    const auto *selected_purpose =
        std::get_if<FixedLockedRuleInput>(&transition.input_basis_refs[3]);
    const auto *selected_rule =
        std::get_if<FixedLockedRuleInput>(&transition.input_basis_refs[4]);
    if (!start || start->source_schema != 1 ||
        !same_source_ref(start->source_ref,
                         construction.execution_binding.source_owner_ref) ||
        !body || !presence || !selected_purpose || !selected_rule ||
        !is_actor_spatial_head_source_kind(presence->source_ref))
      fail("ConstructionTransitionOriginalInputsRequired", id);
    const auto original_body = take(read_character_condition_at_source(
        x.body, body->source_ref, body->source_schema, transition.occurred_at));
    if (original_body.head.actor_id != construction.builder_actor_id)
      fail("ConstructionTransitionBodyActorMismatch", id);
    auto capability = take(read_character_capability_at_head(
        x.body, original_body.head, purpose(x), std::nullopt,
        transition.occurred_at));
    rule(x, capability, id);
    if (selected_purpose->rule_ref != capability.purpose_ref.id ||
        selected_rule->rule_ref != capability.rule_ref.id ||
        !same_original_catalog_identity(selected_purpose->catalog_identity,
                                        x.body.catalog.identity) ||
        !same_original_catalog_identity(selected_rule->catalog_identity,
                                        x.body.catalog.identity))
      fail("ConstructionTransitionLockedRulesMismatch", id);
    const auto location = take(read_actor_spatial_location_from_head_pool(
        x.spatial, construction.builder_actor_id,
        {{presence->source_ref, presence->source_schema}},
        transition.occurred_at, x.military_handoffs));
    at_site(x, terms(x, construction),
            *take(require_actor_ordinary_movement_anchor(location)));
    if (transition.kind == FixedTransitionKind::started) {
      const auto accepted_body = take(read_character_condition_head_source(
          x.body, construction.accepted_condition_head,
          construction.started_at));
      if (compare(transition.occurred_at, construction.started_at) ||
          body->source_schema != accepted_body.source_schema ||
          !same_source_ref(body->source_ref, accepted_body.source) ||
          presence->source_schema !=
              construction.accepted_presence_source.source_schema ||
          !same_source_ref(presence->source_ref,
                           construction.accepted_presence_source.source_ref))
        fail("ConstructionTransitionAcceptedInputsMismatch", id);
    }
    return {true, std::move(capability), {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Status validate_facility_construction_provided_intervals(
    const FacilityConstructionServiceContext &x, const StableId &id,
    const ActualServiceIntervalView &provided) {
  try {
    context(x);
    const auto &construction =
        *take(read_facility_construction_by_id_for_original_dependency(
            x.adoption, id));
    accepted_worker(x, construction);
    if (provided.action_ref != id || !provided.complete)
      fail("ConstructionActualServiceIntervalsRequired", id);
    const auto &accepted = terms(x, construction);
    for (const auto &interval : provided.intervals)
      provided_interval(x, construction, accepted, interval);
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
