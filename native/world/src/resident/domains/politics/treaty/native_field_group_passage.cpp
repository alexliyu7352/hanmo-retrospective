#include "native_field_group_passage.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/military_terms.hpp"
#include <algorithm>
#include <limits>
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
void take(Status result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
}
int compare(const FieldGroupPassageContext &x, const WorldMoment &a,
            const WorldMoment &b) {
  return take(compare_world_moments(a, b, x.arithmetic));
}
bool same_source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool same_carrier(const SpatialCarrierRef &a, const SpatialCarrierRef &b) {
  const auto *aa = std::get_if<ArmyCarrierRef>(&a);
  const auto *bb = std::get_if<ArmyCarrierRef>(&b);
  return aa && bb && aa->army_id == bb->army_id;
}
// 目录锁与时钟证明来自同World，不由调用方提供任意未来M。
WorldMoment now(const FieldGroupPassageContext &x) {
  take(validate_field_group_passage_catalog(x.policies, x.organizations,
                                            x.travel));
  const auto &metadata = x.common.state().metadata;
  const auto &identity = x.policies.identity;
  if (identity.content_id != metadata.content_id ||
      identity.content_version != metadata.content_version ||
      identity.content_digest != metadata.content_digest ||
      identity.ruleset_id != metadata.ruleset_id ||
      !x.read_boundary.bound_to(x.common, x.moment_clock) ||
      !x.formations.bound_to(x.common.state(), x.military))
    fail("FieldPassageSameWorldRequired");
  auto at = take(x.read_boundary.read_observed_moment());
  if (compare(x, at, take(x.clock.read_organization_authority_now())) ||
      compare(x, at, take(x.moment_clock.read_current_moment())))
    fail("FieldPassageClockMismatch");
  return at;
}
std::size_t position(const FieldGroupPassageContext &x,
                     const SourceRef &source) {
  const auto *head = x.common.state().facts.find(source.id);
  const auto &order = x.common.state().facts.order();
  auto i = std::find(order.begin(), order.end(), source.id);
  if (!head || head->id != source.id || head->domain != source.domain ||
      head->kind != source.kind || i == order.end())
    fail("FieldPassageActualSourceRequired", source.id);
  return static_cast<std::size_t>(i - order.begin());
}
// 军属借原成军来源，不从旅程 sponsor 或现任控制者推导。
MilitaryFormationSourceView army_original(const FieldGroupPassageContext &x,
                                          const SpatialCarrierRef &carrier,
                                          const WorldMoment &at) {
  const auto *a = std::get_if<ArmyCarrierRef>(&carrier);
  if (!a)
    fail("FieldPassageArmyCarrierRequired");
  const ArmyRecord *army = nullptr;
  for (const auto &row : x.military.armies)
    if (row.id == a->army_id) {
      if (army)
        fail("FieldPassageArmyNotUnique", a->army_id);
      army = &row;
    }
  if (!army)
    fail("FieldPassageArmyMissing", a->army_id);
  auto original = take(read_original_military_formation(
      x.common.state(), x.military, *army, at, &x.formations, x.arithmetic));
  if (!original.original_terms ||
      original.original_terms->organization_id.empty() ||
      compare(x, original.completed_at, at) > 0)
    fail("FieldPassageOriginalFormationRequired", a->army_id);
  position(x, original.completed_source.source_ref);
  return original;
}
const GroupMovementJourney &journey(const FieldGroupPassageContext &x,
                                    const GroupMovementExecution &execution) {
  const auto *j = x.carriers.journeys.find(execution.journey_id);
  if (!j || j->journey_id != execution.journey_id ||
      !same_carrier(j->carrier, execution.carrier))
    fail("FieldPassageActualJourneyRequired", execution.journey_id);
  return *j;
}
const ActorGroupInitiator &actor_initiator(const GroupMovementJourney &j) {
  const auto *actor = std::get_if<ActorGroupInitiator>(&j.initiator);
  if (!actor || actor->initiator_actor_id.empty() ||
      actor->authority_ref.empty())
    fail("FieldPassageOriginalInitiatorRequired", j.journey_id);
  return *actor;
}
void ordinary_journey(const FieldGroupPassageContext &x,
                      const GroupMovementJourney &j) {
  if (const auto *purpose =
          x.carriers.journey_purpose_bindings.find(j.journey_id))
    if (purpose->journey_id != j.journey_id ||
        !std::holds_alternative<OrdinaryJourneyPurpose>(purpose->purpose))
      fail("FieldPassageNonordinaryJourneyUnsupported", j.journey_id);
}
SourceRef policy_source(const FieldGroupPassagePolicy &p) {
  return {"field_group_passage_catalog", "policy", p.id};
}
const BoundaryCrossingReceipt &crossing(const FieldGroupPassageContext &x,
                                        const SourceRef &ref) {
  if (ref.domain != "travel" || ref.kind != "boundary_crossing")
    fail("FieldPassageCrossingSourceRequired", ref.id);
  position(x, ref);
  const auto *row = x.carriers.crossings.find(ref.id);
  if (!row || row->receipt_id != ref.id)
    fail("FieldPassageCrossingSourceRequired", ref.id);
  return *row;
}
// 原决定已证本fixture非条约后，仍反扫原binding，不把缺provider解释为无后果。
void no_safe_exit(const FieldGroupPassageContext &x,
                  const BoundaryCrossingReceipt &r) {
  if (r.safe_exit_binding_ref || !r.effect_use_refs.empty())
    fail("FieldPassageUnexpectedTreatyEffect", r.receipt_id);
  for (const auto &item : x.safe_exit.bindings.rows())
    if (item.second.entry_receipt_ref.id == r.receipt_id)
      fail("FieldPassageUnexpectedSafeExitBinding", item.first);
}
} // namespace
GroupPoliticalPassageEntryContext NativeFieldGroupPassage::entries() const {
  return {x_.common, x_.decisions, x_.carriers, x_.movement,  x_.executions,
          x_.travel, x_.manifests, x_.clock,    x_.arithmetic};
}
PoliticalPassageContext NativeFieldGroupPassage::policy_context() const {
  auto identity = x_.common.state().identity;
  identity.revision = x_.common.revision();
  identity.tick = x_.common.tick();
  return {identity, x_.read_mode, x_.common, x_.stamps, x_.versions, *this, 2};
}
Status NativeFieldGroupPassage::verify_actual_request(
    const PassageRequest &r, const TransportManifestRead &m) const {
  try {
    auto at = now(x_);
    const auto *execution =
        r.execution ? std::get_if<GroupMovementExecution>(&*r.execution)
                    : nullptr;
    if (!execution || r.purpose != PassagePurpose::enter || r.mode != "army" ||
        !r.army_id || r.fleet_id ||
        !std::holds_alternative<OrdinaryPassageContext>(r.purpose_context) ||
        !same_carrier(r.carrier, execution->carrier) ||
        !same_carrier(r.carrier, m.carrier))
      fail("FieldPassageOrdinaryArmyRequestRequired");
    const auto &j = journey(x_, *execution);
    ordinary_journey(x_, j);
    const auto &initiator = actor_initiator(j);
    const auto *requester = std::get_if<ActorPassageRequester>(&r.requester);
    const auto *army = std::get_if<ArmyCarrierRef>(&r.carrier);
    if (!requester ||
        requester->acting_actor_id != initiator.initiator_actor_id || !army ||
        army->army_id != *r.army_id ||
        j.composition_revision != m.composition_revision ||
        r.window.from_tick != at.half_day_tick)
      fail("FieldPassageOriginalRequestMismatch", j.journey_id);
    if (r.manifest_basis.source_refs.size() != m.source_refs.size())
      fail("FieldPassageManifestBasisMismatch");
    for (std::size_t i = 0; i < m.source_refs.size(); ++i)
      if (!same_source(r.manifest_basis.source_refs[i], m.source_refs[i]))
        fail("FieldPassageManifestBasisMismatch");
    auto actual = take(read_actual_group_passage_entry(
        x_.carriers, x_.movement, x_.travel, *execution, r.boundary, at,
        x_.arithmetic));
    if (actual.manifest_revision != m.manifest_revision ||
        actual.composition_revision != m.composition_revision ||
        compare(x_, actual.occurred_at, at))
      fail("FieldPassageActualEntryMismatch", j.journey_id);
    auto original = army_original(x_, r.carrier, at);
    if (r.subject.kind != SubjectKind::organization ||
        r.subject.id != original.original_terms->organization_id)
      fail("FieldPassageOriginalArmyOrganizationMismatch");
    take(
        read_field_group_passage_policy(x_.policies, r.boundary, r.subject.id));
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<PoliticalPassagePolicyRead>
NativeFieldGroupPassage::read_current_policy(
    const PassageRequest &r, const TransportManifestRead &m) const {
  try {
    take(verify_actual_request(r, m));
    const auto *p = take(
        read_field_group_passage_policy(x_.policies, r.boundary, r.subject.id));
    PoliticalPassagePolicyRead result;
    result.identity = policy_context().identity;
    result.admission_source = policy_source(*p);
    result.admission = PassageAdmission::allowed;
    result.visible_reason_code = "field_fixture_explicit_army_permission";
    result.authority_basis = ExistingOriginalPassageBasis{
        p->id, p->version, x_.policies.identity.content_digest};
    return {true, std::move(result), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status
NativeFieldGroupPassage::verify_tariff_payee(const PassageBoundaryRef &,
                                             const PassageTariff &) const {
  return common_failure("FieldPassageFixtureHasNoTariff");
}
Outcome<std::vector<PreparedGroupPassage>>
NativeFieldGroupPassage::prepare_actual_entry(
    const GroupMovementJourney &supplied, const TransportManifestRead &manifest,
    const MovementBasis &basis, const WorldMoment &at) const {
  try {
    if (compare(x_, at, now(x_)))
      fail("FieldPassageActualNowRequired");
    const auto &j = journey(x_, supplied.movement_execution);
    if (j.position_ref != supplied.position_ref ||
        j.composition_revision != supplied.composition_revision ||
        j.movement_prefix_ref != supplied.movement_prefix_ref ||
        j.movement_execution.segment_ordinal !=
            supplied.movement_execution.segment_ordinal ||
        !same_carrier(j.carrier, supplied.carrier))
      fail("FieldPassageOriginalJourneyMismatch", supplied.journey_id);
    const auto *prefix = x_.movement.prefixes.find(j.movement_prefix_ref);
    const auto *original_basis =
        prefix ? std::get_if<RoadMovementBasis>(&prefix->basis) : nullptr;
    const auto *supplied_basis = std::get_if<RoadMovementBasis>(&basis);
    if (!original_basis || !supplied_basis ||
        original_basis->basis_ref != supplied_basis->basis_ref ||
        original_basis->original_unit_ref != supplied_basis->original_unit_ref)
      fail("FieldPassageActualBasisRequired", j.journey_id);
    auto boundary = take(read_actual_group_road_entry_boundary(
        x_.carriers, x_.movement, x_.travel, j.movement_execution, at,
        x_.arithmetic));
    if (!boundary)
      return {true, {}, {}}; // 原S1明确已在边内；不是以空结果跳过新的边界。
    if (at.half_day_tick == std::numeric_limits<Integer>::max())
      fail("FieldPassageWindowOverflow");
    ordinary_journey(x_, j);
    const auto &actor = actor_initiator(j);
    auto original = army_original(x_, j.carrier, at);
    PassageRequest r;
    r.identity = policy_context().identity;
    r.read_stamp = manifest.read_stamp;
    r.subject = {SubjectKind::organization,
                 original.original_terms->organization_id};
    r.requester = ActorPassageRequester{actor.initiator_actor_id};
    r.carrier = j.carrier;
    r.manifest_basis = {manifest.manifest_revision,
                        manifest.composition_revision, manifest.source_refs};
    r.boundary = *boundary;
    r.window = {at.half_day_tick, at.half_day_tick + 1};
    r.mode = "army";
    r.army_id = std::get<ArmyCarrierRef>(j.carrier).army_id;
    r.purpose = PassagePurpose::enter;
    r.execution = j.movement_execution;
    auto decision =
        take(evaluate_political_passage(policy_context(), r, manifest));
    std::vector<PreparedGroupPassage> result;
    result.push_back({std::move(r), std::move(decision)});
    return {true, std::move(result), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<SourceRef> NativeFieldGroupPassage::adopt_actual_decision_in_candidate(
    CommonStore &common, const PreparedGroupPassage &prepared) {
  try {
    if (&common != &x_.common || !x_.candidate_decisions ||
        x_.candidate_decisions != &x_.decisions)
      fail("FieldPassageCandidateMismatch");
    const auto *execution =
        prepared.request.execution
            ? std::get_if<GroupMovementExecution>(&*prepared.request.execution)
            : nullptr;
    if (!execution)
      fail("FieldPassageGroupExecutionRequired");
    auto actual = take(read_actual_group_passage_entry(
        x_.carriers, x_.movement, x_.travel, *execution,
        prepared.request.boundary, now(x_), x_.arithmetic));
    const auto *captured = take(read_historical_transport_entry_manifest(
        common, x_.carriers, x_.movement, x_.executions, x_.manifests,
        *prepared.request.execution, actual.entry_ordinal,
        prepared.request.boundary, actual.manifest_revision,
        actual.composition_revision, x_.arithmetic));
    auto context = entries();
    NativeGroupPoliticalPassageEntryRead entry(context);
    // 再算真实采用条件，不把prepared结果作为已授权缓存。
    // 持久manifest故意不保存stamp；这里仅借原prepare的同一热stamp，
    // NativeGroupPoliticalPassageEntryRead仍逐字段对比真实captured，不自行发行票据。
    auto manifest = captured->manifest;
    manifest.read_stamp = prepared.request.read_stamp;
    take(verify_actual_request(prepared.request, manifest));
    auto result = take(adopt_political_passage_decision_in_candidate(
        common, *x_.candidate_decisions, policy_context(), entry,
        prepared.request, manifest));
    take(validate_historical_decision(
        *take(read_political_passage_decision(common, x_.decisions, result))));
    return {true, std::move(result), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status NativeFieldGroupPassage::validate_historical_decision(
    const PoliticalPassageDecisionSource &supplied) const {
  try {
    auto current = now(x_);
    const SourceRef ref{"politics", "political_passage_decided", supplied.id};
    const auto *row =
        take(read_political_passage_decision(x_.common, x_.decisions, ref));
    if (row != &supplied)
      fail("FieldPassageStoredDecisionRequired", supplied.id);
    const auto &d = row->decision;
    take(validate_political_passage_authority_payload(d));
    take(validate_political_passage_decision_source_time(*row));
    if (d.identity.world_id != x_.common.state().identity.world_id ||
        d.identity.revision < 0 || d.identity.revision > x_.common.revision() ||
        d.entry_tick == std::numeric_limits<Integer>::max() ||
        d.requested_window.until_tick != d.entry_tick + 1)
      fail("FieldPassageOriginalIdentityWindowMismatch", row->id);
    const auto *original =
        std::get_if<ExistingOriginalPassageBasis>(&d.authority_basis);
    if (d.source_schema_version != 2 || !original ||
        !std::holds_alternative<OrdinaryPassageContext>(d.purpose_context) ||
        d.mode != "army" || d.subject.kind != SubjectKind::organization ||
        d.admission != PassageAdmission::allowed || d.toll ||
        !d.authority_basis_refs.empty() || !d.treaty_term_refs.empty() ||
        !d.local_delivery_source_refs.empty() ||
        d.visible_reason_code != "field_fixture_explicit_army_permission" ||
        compare(x_, row->occurred_at, current) > 0)
      fail("FieldPassageOriginalDecisionMismatch", row->id);
    const auto *p = take(
        read_field_group_passage_policy(x_.policies, d.boundary, d.subject.id));
    if (original->policy_id != p->id ||
        original->policy_version != p->version ||
        original->catalog_digest != x_.policies.identity.content_digest ||
        !same_source(d.admission_source, policy_source(*p)))
      fail("FieldPassageLockedPolicyMismatch", row->id);
    auto formation = army_original(x_, row->carrier, row->occurred_at);
    if (formation.original_terms->organization_id != d.subject.id ||
        position(x_, formation.completed_source.source_ref) >=
            position(x_, ref))
      fail("FieldPassageFormationBeforeDecisionRequired", row->id);
    const auto *execution =
        std::get_if<GroupMovementExecution>(&row->execution);
    if (!execution || !same_carrier(execution->carrier, row->carrier))
      fail("FieldPassageGroupExecutionRequired", row->id);
    const auto &j = journey(x_, *execution);
    ordinary_journey(x_, j);
    const auto &actor = actor_initiator(j);
    const auto *requester = std::get_if<ActorPassageRequester>(&d.requester);
    if (!requester || requester->acting_actor_id != actor.initiator_actor_id)
      fail("FieldPassageOriginalRequesterMismatch", row->id);
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<ActualTransportExpense>
NativeFieldGroupPassage::read_adopted_toll(const SourceRef &ref,
                                           const GroupMovementJourney &j,
                                           const WorldMoment &at) const {
  auto context = entries();
  return read_group_political_toll_at_adoption(context, *this, ref, j, at);
}
Status NativeFieldGroupPassage::validate_crossing_decision(
    const BoundaryCrossingReceipt &r) const {
  auto context = entries();
  // 同一读口同时服务原采用前草稿与稳定历史；只有本 World 的原表借用走历史门。
  // 同 ID 的副本/异 World 行仍走原 pending 拒门，不能借存在的头冒充本表来源。
  if (x_.carriers.crossings.find(r.receipt_id) == &r)
    return validate_after_positive_crossing(r);
  return validate_pending_group_political_crossing(context, *this, x_.payments,
                                                   r);
}
Status NativeFieldGroupPassage::validate_after_positive_crossing(
    const BoundaryCrossingReceipt &r) const {
  try {
    now(x_);
    if (&crossing(x_, {"travel", "boundary_crossing", r.receipt_id}) != &r)
      fail("FieldPassageBorrowedCrossingRequired", r.receipt_id);
    auto context = entries();
    take(validate_original_group_political_crossing(context, *this, x_.payments,
                                                    r));
    auto span = take(read_actual_group_crossing_first_positive_span(
        x_.common, x_.carriers, x_.movement, x_.executions, x_.travel, r,
        x_.arithmetic));
    if (compare(x_, span.actual_span.to, now(x_)) > 0)
      fail("FieldPassageFuturePositiveSpan", r.receipt_id);
    no_safe_exit(x_, r);
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<std::optional<SourceRef>>
NativeFieldGroupPassage::adopt_after_positive_crossing_in_candidate(
    CommonStore &common, const SourceRef &ref) {
  try {
    if (&common != &x_.common || !x_.candidate_decisions ||
        x_.candidate_decisions != &x_.decisions)
      fail("FieldPassageCandidateMismatch");
    take(validate_after_positive_crossing(crossing(x_, ref)));
    return {true, std::nullopt, {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status NativeFieldGroupPassage::validate_after_boundary_completion(
    const BoundaryCrossingReceipt &r) const {
  try {
    take(validate_after_positive_crossing(r));
    auto completed = take(read_actual_group_boundary_completion(
        x_.common, x_.carriers, x_.movement, x_.executions, x_.travel, r, x_.arithmetic));
    if (completed && compare(x_, completed->completed_at, now(x_)) > 0)
      fail("FieldPassageFutureBoundaryCompletion", r.receipt_id);
    no_safe_exit(x_, r);
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status NativeFieldGroupPassage::adopt_after_boundary_completion_in_candidate(
    CommonStore &common, const SourceRef &ref) {
  try {
    if (&common != &x_.common || !x_.candidate_decisions ||
        x_.candidate_decisions != &x_.decisions)
      fail("FieldPassageCandidateMismatch");
    return validate_after_boundary_completion(crossing(x_, ref));
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status NativeFieldGroupPassage::validate_all() const {
  try {
    now(x_);
    auto context = entries();
    NativeGroupPoliticalPassageEntryRead entry(context);
    take(validate_political_passage_sources(x_.common, x_.decisions, entry,
                                            *this));
    for (const auto &h : x_.common.state().facts.rows())
      if (h.second.domain == "politics" &&
          h.second.kind == "political_passage_decided") {
        const auto *row = x_.decisions.decisions.find(h.first);
        if (!row || row->id != h.first)
          fail("FieldPassageOrphanDecision", h.first);
      }
    for (const auto &r : x_.carriers.crossings.rows())
      take(validate_after_boundary_completion(r.second));
    if (!x_.safe_exit.bindings.rows().empty() ||
        !x_.safe_exit.lines.rows().empty())
      fail("FieldPassageUnexpectedSafeExitState");
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
} // namespace hanmo::resident
