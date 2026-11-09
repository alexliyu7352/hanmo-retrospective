// 本 TU 先引入真实复制租约定义；保留原同 job 字段与生命周期。
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "military_availability.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ordinary_loss.hpp"
#include "resident/domains/military/military_model.hpp"
#include "character_condition_read.hpp"
#include "condition_application_fact.hpp"
#include "created_origin.hpp"
#include "named_custody_availability_internal.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
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
template <class T, class F> Outcome<T> checked(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool allocated(const CommonStore &c, const StableId &id) {
  constexpr char prefix[] = "capacity:";
  if (id.rfind(prefix, 0))
    return false;
  Integer value = 0;
  const auto *first = id.data() + sizeof(prefix) - 1;
  const auto *end = id.data() + id.size();
  auto read = std::from_chars(first, end, value);
  return read.ec == std::errc{} && read.ptr == end && value > 0 &&
         value < c.state().next_instance_id &&
         std::to_string(value) == id.substr(sizeof(prefix) - 1);
}
MilitaryActorConsequence source(const CharacterConditionSources &body,
                                const MilitaryAvailabilityCauseV2 &cause) {
  if (!body.military || !body.military_exact || cause.source_schema != 2)
    fail("MilitaryAvailabilitySourceRequired", cause.casualty_source.id);
  const MilitaryCasualtyHistoryContext history{
      body.common, body.actual_now, body.military_formations,
      body.military_returns, body.military_history_order};
  auto actual = take(read_military_actor_consequence(
      *body.military, *body.military_exact, cause.casualty_source,
      cause.source_schema, cause.source_ordinal, {}, &history));
  if (actual.status != "captured" && actual.status != "deserted")
    fail("MilitaryAvailabilityDispositionRequired", cause.casualty_source.id);
  return actual;
}
// 只借原段与唯一伤亡行；captured 仍走原完整俘方证明，逃散不冒俘虏。
struct FieldAvailabilitySourceRead {
  const WorldMoment *applied_at = nullptr;
  const MilitaryActorChange *actor_change = nullptr;
  Integer source_ordinal = 0;
};
FieldAvailabilitySourceRead field_source(const CharacterConditionSources &body,
                                         const SourceRef &ref,
                                         Integer ordinal) {
  if (!take(condition_application_facts_selected(
          body.catalog.identity,
          ConditionApplicationHistoryOwner::injury_sources)) ||
      !body.military || !body.injury_field || !body.injury_field_clock ||
      ref.domain != "military" || ref.kind != "military_casualties" ||
      ordinal < 0 ||
      !body.injury_field_clock->bound_to(body.common, *body.military,
                                        *body.injury_field))
    fail("FieldAvailabilityOriginalSourceRequired", ref.id);
  const MilitaryCasualtyFact *receipt = nullptr;
  for (const auto &r : body.military->casualty_facts)
    if (r.id == ref.id) {
      if (receipt)
        fail("FieldAvailabilityDuplicateSource", ref.id);
      receipt = &r;
    }
  if (!receipt ||
      static_cast<std::size_t>(ordinal) >= receipt->actor_changes.size())
    fail("FieldAvailabilityOriginalOrdinalRequired", ref.id);
  const auto &change =
      receipt->actor_changes[static_cast<std::size_t>(ordinal)];
  if (change.status == "captured") {
    const auto actual = take(read_field_segment_captivity_source(
        body.common, *body.military, *body.injury_field, ref, change.actor_id,
        *body.injury_field_clock));
    const auto now = take(body.injury_field_clock->read_current_moment());
    if (actual.source_ordinal != ordinal || actual.actor_change != &change ||
        take(compare_world_moments(now, body.actual_now)) != 0)
      fail("FieldAvailabilityOriginalMomentMismatch", ref.id);
    return {&actual.source.settlement->applied_at, actual.actor_change, actual.source_ordinal};
  }
  if (change.status != "deserted" || change.actor_id.empty() ||
      !body.common.actor(change.actor_id) || !change.captor_army_id.empty() ||
      change.garrison_id)
    fail("FieldAvailabilityDispositionRequired", ref.id);
  const auto now = take(body.injury_field_clock->read_current_moment());
  if (take(compare_world_moments(now, body.actual_now)) != 0)
    fail("FieldAvailabilityOriginalMomentMismatch", ref.id);
  // 新截止步是自己的真实父源，不伪装成终局段；低层不回调完整Field几何。
  const auto *parent_head = body.common.state().facts.find(receipt->battle_id);
  if (parent_head && parent_head->domain == "military" &&
      parent_head->kind == "field_army_exit_step") {
    const auto original = take(read_field_army_exit_ordinary_casualty_source(
        body.common, *body.military, *body.injury_field, ref, *body.injury_field_clock));
    if (original.casualties != receipt)
      fail("FieldAvailabilityOriginalParentMismatch", ref.id);
    return {&original.step->applied_at, &change, ordinal};
  }
  const auto parent_memory = CheckpointMemoryCharge(0);
  const SourceRef parent{checkpoint_fresh_text_copy("military"),
                         checkpoint_fresh_text_copy("field_battle_segment"),
                         checkpoint_fresh_string_copy(receipt->battle_id)};
  const auto original = take(read_field_segment_settlement_source(
      body.common, *body.military, *body.injury_field, parent,
      body.injury_field_clock));
  // 原父读口已核唯一 actor、名册分区、原段头序及实际 Army；不从今日状态倒证。
  if (original.casualties != receipt ||
      take(compare_world_moments(original.settlement->applied_at, now)) > 0)
    fail("FieldAvailabilityOriginalMomentMismatch", ref.id);
  return {&original.settlement->applied_at, &change, ordinal};
}
std::size_t position(const CommonStore &common, const SourceRef &source) {
  const auto actual = read_actual_common_fact_position(common, source);
  if (!actual.ok)
    fail("MilitaryAvailabilityActualHeaderRequired", source.id);
  return actual.value;
}
void validate_original(const CharacterConditionSources &body,
                       const CapacityTransitionV2 &row) {
  const auto *original =
      std::get_if<MilitaryAvailabilityCauseV2>(&row.cause_ref);
  if (!original)
    fail("MilitaryAvailabilityCauseRequired", row.transition_id);
  if (original->source_schema == 1) {
    const auto actual =
        field_source(body, original->casualty_source, original->source_ordinal);
    if (!allocated(body.common, row.transition_id) ||
        row.actor_id != actual.actor_change->actor_id ||
        row.purpose.id != "original_military_availability" ||
        row.purpose.version != 1 ||
        row.purpose.catalog_digest != body.catalog.identity.content_digest ||
        row.to || row.expected_recovery_at ||
        row.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        take(compare_world_moments(row.effective_at.exact_moment,
                                   *actual.applied_at)) != 0)
      fail("FieldAvailabilityTransitionMismatch", row.transition_id);
    position(body.common, original->casualty_source);
    return;
  }
  auto actual = source(body, *original);
  if (!allocated(body.common, row.transition_id) ||
      row.actor_id != actual.actor_id ||
      row.purpose.id != "original_military_availability" ||
      row.purpose.version != 1 ||
      row.purpose.catalog_digest != body.catalog.identity.content_digest ||
      row.to || row.expected_recovery_at ||
      row.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      take(compare_world_moments(row.effective_at.exact_moment,
                                 actual.occurred_at)) ||
      take(compare_world_moments(actual.occurred_at, body.actual_now)) > 0)
    fail("MilitaryAvailabilityTransitionMismatch", row.transition_id);
  position(body.common, original->casualty_source);
}
bool availability(const CapacityTransitionV2 &row) {
  return std::holds_alternative<MilitaryAvailabilityCauseV2>(row.cause_ref) ||
         std::holds_alternative<NamedMilitaryCaptureAvailabilityCause>(
             row.cause_ref) ||
         std::holds_alternative<NamedMilitaryReleaseAvailabilityCause>(
             row.cause_ref);
}
MilitaryAvailabilityCausePosition
cause_position(const CapacityTransitionCauseV2 &cause) {
  if (const auto *v = std::get_if<MilitaryAvailabilityCauseV2>(&cause))
    return {&v->casualty_source, v->source_schema, v->source_ordinal};
  if (const auto *v =
          std::get_if<NamedMilitaryCaptureAvailabilityCause>(&cause))
    return {&v->term_source, v->source_schema, 0};
  if (const auto *v =
          std::get_if<NamedMilitaryReleaseAvailabilityCause>(&cause))
    return {&v->release_source, v->source_schema, 0};
  fail("MilitaryAvailabilityCauseRequired");
}
void validate(const CharacterConditionSources &body,
              const CapacityTransitionV2 &row) {
  if (!allocated(body.common, row.transition_id))
    fail("MilitaryAvailabilityKeyMismatch", row.transition_id);
  if (std::holds_alternative<MilitaryAvailabilityCauseV2>(row.cause_ref))
    validate_original(body, row);
  else
    take(validate_named_custody_availability_transition(body, row));
}
bool same(const CapacityTransitionCauseV2 &cause, const SourceRef &ref,
          Integer ordinal) {
  if (std::holds_alternative<ConditionPurposeCapacityCause>(cause))
    return false;
  const auto a = cause_position(cause);
  return a.ordinal == ordinal && a.source->domain == ref.domain &&
         a.source->kind == ref.kind && a.source->id == ref.id;
}
void actor_history(const CharacterConditionSources &body,
                   const StableId &actor_id) {
  const auto *actor = body.common.actor(actor_id);
  if (!actor)
    fail("CharacterAvailabilityActorRequired", actor_id);
  const auto &rows =
      body.state.character_condition_extensions.capacity_transitions;
  const auto initial = take(read_character_initial_values_v2(
      body.common, body.state, body.catalog, body.family_catalog, body.shared,
      actor_id, body.actual_now, body.born_sources));
  bool available = initial.available;
  std::optional<std::pair<std::size_t, Integer>> previous;
  std::optional<WorldMoment> previous_at;
  const StableId *previous_id = nullptr;
  for (const auto &entry : rows.rows())
    if (entry.second.actor_id == actor_id && availability(entry.second)) {
      const auto &value = entry.second;
      if (entry.first != value.transition_id)
        fail("MilitaryAvailabilityKeyMismatch", entry.first);
      validate(body, value);
      const auto cause = cause_position(value.cause_ref);
      std::size_t count = 0;
      for (const auto &other : rows.rows())
        if (same(other.second.cause_ref, *cause.source, cause.ordinal))
          ++count;
      if (count != 1)
        fail("DuplicateMilitaryAvailabilityCause", entry.first);
      const auto index =
          std::make_pair(position(body.common, *cause.source), cause.ordinal);
      if ((previous && index <= *previous) ||
          (previous_at &&
           take(compare_world_moments(*previous_at,
                                      value.effective_at.exact_moment)) > 0) ||
          value.from != available)
        fail("MilitaryAvailabilityActualChainMismatch", entry.first);
      if (const auto *capture =
              std::get_if<NamedMilitaryCaptureAvailabilityCause>(
                  &value.cause_ref)) {
        if (bool(capture->predecessor_transition_id) != bool(previous_id) ||
            (previous_id &&
             *capture->predecessor_transition_id != *previous_id))
          fail("NamedCustodyAvailabilityOriginalPredecessorMismatch",
               entry.first);
      }
      if (const auto *release =
              std::get_if<NamedMilitaryReleaseAvailabilityCause>(
                  &value.cause_ref))
        if (!previous_id || release->capture_transition_id != *previous_id)
          fail("NamedCustodyAvailabilityInterleavedCause", entry.first);
      available = value.to;
      previous = index;
      previous_at = value.effective_at.exact_moment;
      previous_id = &entry.first;
    }
  // 死亡仍只移除availability；看管解除不触碰真实生命、伤病或疲劳。
  if (!actor->alive)
    available = false;
  if (actor->available != available)
    fail("CharacterAvailabilityUnexplainedChange", actor_id);
}
} // namespace
SourceRef military_availability_transition_source(const StableId &id) {
  return {"actor_foundation", "capacity_transition", id};
}
Status
validate_military_availability_coverage(const CharacterConditionSources &body) {
  return checked<Done>([&] {
    const auto &rows =
        body.state.character_condition_extensions.capacity_transitions;
    for (const auto &row : rows.rows())
      if (availability(row.second)) {
        if (row.first != row.second.transition_id)
          fail("MilitaryAvailabilityKeyMismatch", row.first);
        validate(body, row.second);
        const auto original = cause_position(row.second.cause_ref);
        std::size_t count = 0;
        for (const auto &other : rows.rows())
          if (same(other.second.cause_ref, *original.source, original.ordinal))
            ++count;
        if (count != 1)
          fail("DuplicateMilitaryAvailabilityCause", row.first);
      }
    take(validate_named_custody_availability_coverage(body));
    if (body.military_exact)
      for (const auto &row : body.military_exact->dynamic_casualty_facts.rows())
        for (std::size_t n = 0; n < row.second.actor_changes.size(); ++n) {
          const auto &actual = row.second.actor_changes[n];
          if (actual.status != "captured" && actual.status != "deserted")
            continue;
          std::size_t count = 0;
          for (const auto &transition : rows.rows())
            if (same(transition.second.cause_ref,
                     {"military", "military_casualties", row.first},
                     static_cast<Integer>(n)))
              ++count;
          if (count != 1)
            fail("MilitaryAvailabilitySourceCoverageMismatch", row.first);
        }
    if (take(condition_application_facts_selected(
            body.catalog.identity,
            ConditionApplicationHistoryOwner::injury_sources))) {
      if (!body.military || !body.injury_field || !body.injury_field_clock)
        fail("FieldAvailabilityCoverageSourcesRequired");
      for (const auto &receipt : body.military->casualty_facts) {
        const auto *parent = body.common.state().facts.find(receipt.battle_id);
        if (!parent || parent->domain != "military" ||
            (parent->kind != "field_battle_segment" &&
             parent->kind != "field_army_exit_step"))
          continue;


        const auto memory = CheckpointMemoryCharge(0);
        SourceRef ref{checkpoint_fresh_text_copy("military"),
                      checkpoint_fresh_text_copy("military_casualties"),
                      checkpoint_fresh_string_copy(receipt.id)};
        for (std::size_t n = 0; n < receipt.actor_changes.size(); ++n) {
          if (receipt.actor_changes[n].status != "captured" &&
              receipt.actor_changes[n].status != "deserted")
            continue;
          if (n > static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
            fail("FieldAvailabilityOrdinalOverflow", receipt.id);
          const auto ordinal = static_cast<Integer>(n);
          field_source(body, ref, ordinal);
          std::size_t count = 0;
          for (const auto &row : rows.rows())
            if (same(row.second.cause_ref, ref, ordinal)) {
              const auto *cause = std::get_if<MilitaryAvailabilityCauseV2>(
                  &row.second.cause_ref);
              if (!cause || cause->source_schema != 1)
                fail("FieldAvailabilitySourceSchemaMismatch", receipt.id);
              ++count;
            }
          if (count != 1)
            fail("FieldAvailabilitySourceCoverageMismatch", receipt.id);
        }
      }
    }
    for (const auto &actor : body.common.state().actors.rows())
      actor_history(body, actor.first);
    return Done{};
  });
}
Outcome<const CapacityTransitionV2 *>
read_verified_military_availability_transition(
    const CharacterConditionSources &body, const StableId &id) {
  return checked<const CapacityTransitionV2 *>([&] {
    take(validate_military_availability_coverage(body));
    const auto *row =
        body.state.character_condition_extensions.capacity_transitions.find(id);
    if (!row || !availability(*row))
      fail("MilitaryAvailabilityTransitionMissing", id);
    return row;
  });
}
// 原成员损失只借已落账这一笔的实际 Field 因果，不把未来解除看管作为其前置。
Outcome<const CapacityTransitionV2 *>
read_original_field_availability_transition_source(
    const CharacterConditionSources &body, const SourceRef &ref, Integer schema) {
  if (ref.domain != "actor_foundation" || ref.kind != "capacity_transition" || schema != 2)
    return {false, nullptr,
            common_failure("MilitaryAvailabilityTypedSourceRequired", ref.id,
                           ErrorKind::unsupported_capability).error};
  return checked<const CapacityTransitionV2 *>([&] {
    const auto &rows = body.state.character_condition_extensions.capacity_transitions;
    const auto *row = rows.find(ref.id);
    if (!row || row->transition_id != ref.id)
      fail("MilitaryAvailabilityTransitionMissing", ref.id);
    const auto *cause = std::get_if<MilitaryAvailabilityCauseV2>(&row->cause_ref);
    if (!cause || cause->source_schema != 1)
      fail("FieldAvailabilityOriginalSourceRequired", ref.id);
    // 复用原真正 Field 段/截止步、唯一伤亡 ordinal、身体行与应用 M 校验。
    validate_original(body, *row);
    std::size_t count = 0;
    for (const auto &[id, other] : rows.rows())
      if (const auto *original = std::get_if<MilitaryAvailabilityCauseV2>(&other.cause_ref))
        if (original->casualty_source.domain == cause->casualty_source.domain &&
            original->casualty_source.kind == cause->casualty_source.kind &&
            original->casualty_source.id == cause->casualty_source.id &&
            original->source_ordinal == cause->source_ordinal) {
          if (id != other.transition_id)
            fail("MilitaryAvailabilityKeyMismatch", id);
          ++count;
        }
    if (count != 1) fail("DuplicateMilitaryAvailabilityCause", ref.id);
    return row;
  });
}
Outcome<const CapacityTransitionV2 *>
read_verified_military_availability_transition_provenance(
    const CharacterConditionSources &body, const SourceRef &ref,
    Integer schema) {
  if (ref.domain != "actor_foundation" || ref.kind != "capacity_transition" ||
      schema != 2)
    return {false, nullptr,
            common_failure("MilitaryAvailabilityTypedSourceRequired", ref.id,
                           ErrorKind::unsupported_capability)
                .error};
  return checked<const CapacityTransitionV2 *>([&] {
    const auto *row =
        body.state.character_condition_extensions.capacity_transitions.find(
            ref.id);
    if (!row || row->transition_id != ref.id || !availability(*row))
      fail("MilitaryAvailabilityTransitionMissing", ref.id);
    // This is the same original chain body used by the availability reducer:
    // actual cause/source, unique ordinal/order, predecessor and current state.
    actor_history(body, row->actor_id);
    return row;
  });
}
Outcome<MilitaryAvailabilityApplication> apply_verified_military_availability(
    CommonStore &common, AuthoredActorFoundationState &state,
    const CharacterConditionSources &body, const SourceRef &ref,
    Integer ordinal, const ReadStamp &stamp,
    const ReadStampAuthority &authority,
    const DomainReadVersionSource &versions) {
  return checked<MilitaryAvailabilityApplication>([&] {
    if (&body.common != &common || &body.state != &state)
      fail("MilitaryAvailabilitySameCandidateRequired", ref.id);
    MilitaryAvailabilityCauseV2 cause{ref, 2, ordinal};
    auto actual = source(body, cause);
    auto &rows = state.character_condition_extensions.capacity_transitions;
    const CapacityTransitionV2 *prior = nullptr;
    for (const auto &row : rows.rows())
      if (same(row.second.cause_ref, ref, ordinal)) {
        if (prior)
          fail("DuplicateMilitaryAvailabilityCause", ref.id);
        prior = &row.second;
      }
    if (prior) {
      validate(body, *prior);
      return MilitaryAvailabilityApplication{prior->transition_id, {}, true};
    }
    take(authority.validate(stamp, common.state().identity,
                            ReadMode::phase_current, common, versions));
    bool actor_read = false;
    for (const auto &resource : stamp.resources())
      if (resource.resource.kind == ResourceKey::Kind::actor &&
          resource.resource.id == actual.actor_id)
        actor_read = true;
    if (!actor_read ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  ref.id) == stamp.source_ids().end() ||
        take(compare_world_moments(stamp.span().to, actual.occurred_at)) ||
        take(compare_world_moments(body.actual_now, actual.occurred_at)))
      fail("MilitaryAvailabilityActualReadMismatch", ref.id);
    const auto *actor = common.actor(actual.actor_id);
    if (!actor || !actor->alive)
      fail("MilitaryAvailabilityLivingActorRequired", actual.actor_id);
    const auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("MilitaryAvailabilityCaptureRequired", actual.actor_id);
    const auto index = std::make_pair(position(common, ref), ordinal);
    for (const auto &row : rows.rows())
      if (row.second.actor_id == actual.actor_id && availability(row.second)) {
        const auto original = cause_position(row.second.cause_ref);
        if (std::make_pair(position(common, *original.source),
                           original.ordinal) >= index)
          fail("MilitaryAvailabilitySourceAlreadyPassed", ref.id);
      }
    CapacityTransitionV2 transition;
    transition.transition_id = take(common.next_id("capacity"));
    transition.actor_id = actual.actor_id;
    transition.purpose = {"original_military_availability", 1,
                          body.catalog.identity.content_digest};
    transition.from = actor->available;
    transition.to = false;
    transition.effective_at.exact_moment = actual.occurred_at;
    transition.cause_ref = std::move(cause);
    validate(body, transition);
    const auto offset = common.actor_condition_execution_drafts().size();
    ActorMutation mutation;
    mutation.actor_id = actual.actor_id;
    mutation.available = false;
    take(common.apply_actor(mutation));
    const auto &drafts = common.actor_condition_execution_drafts();
    if (drafts.size() != offset + 1 || !drafts[offset].available_written ||
        !drafts[offset].before ||
        drafts[offset].before->available != transition.from ||
        drafts[offset].after.available || drafts[offset].hp_written ||
        drafts[offset].fatigue_written || drafts[offset].injury_written ||
        drafts[offset].alive_written)
      fail("MilitaryAvailabilityActualWriteMismatch", actual.actor_id);
    const auto id = transition.transition_id;
    if (!rows.insert(id, std::move(transition)))
      fail("DuplicateCapacityTransition", id);
    return MilitaryAvailabilityApplication{
        id, {{id, capture, static_cast<Integer>(offset + 1)}}, false};
  });
}
} // namespace hanmo::resident
#include "condition_source_retention.hpp"
namespace hanmo::resident {
Status
visit_military_availability_source_pins(const CharacterConditionSources &body,
                                        CharacterConditionPinSink &sink) {
  auto valid = validate_military_availability_coverage(body);
  if (!valid.ok)
    return valid;
  for (const auto &row :
       body.state.character_condition_extensions.capacity_transitions.rows())
    if (availability(row.second)) {
      const auto original = cause_position(row.second.cause_ref);
      valid = sink.actual_pin(
          {{"actor_foundation", "capacity_transition", row.first, "cause_ref"},
           *original.source,
           SourcePinReason::dependent_state});
      if (!valid.ok)
        return valid;
      const StableId *predecessor = nullptr;
      if (const auto *capture =
              std::get_if<NamedMilitaryCaptureAvailabilityCause>(
                  &row.second.cause_ref);
          capture && capture->predecessor_transition_id)
        predecessor = &*capture->predecessor_transition_id;
      if (const auto *release =
              std::get_if<NamedMilitaryReleaseAvailabilityCause>(
                  &row.second.cause_ref))
        predecessor = &release->capture_transition_id;
      if (predecessor) {
        valid = sink.actual_pin(
            {{"actor_foundation", "capacity_transition", row.first,
              "availability_predecessor"},
             {"actor_foundation", "capacity_transition", *predecessor},
             SourcePinReason::dependent_state});
        if (!valid.ok)
          return valid;
      }
    }
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status validate_military_availability_actor_history(
    const CharacterConditionSources &body, const StableId &actor) {
  return checked<Done>([&] {
    actor_history(body, actor);
    return Done{};
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
bool is_original_availability_transition(
    const CapacityTransitionV2 &row) noexcept {
  return availability(row);
}
Outcome<MilitaryAvailabilityCausePosition>
read_original_availability_cause_position(const CharacterConditionSources &body,
                                          const CapacityTransitionV2 &row) {
  return checked<MilitaryAvailabilityCausePosition>([&] {
    validate(body, row);
    return cause_position(row.cause_ref);
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
struct FieldMilitaryAvailabilityPermit::Impl : CheckpointAllocatedObject {
  CommonStore *common;
  AuthoredActorFoundationState *foundation;
  const ActorFoundationCatalog *catalog;
  SourceRef source;
  Integer ordinal;
  const MilitaryActorChange *actual_actor;
  WorldMoment at;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture;
  ActorConditionSavepointBoundary savepoint;
  std::size_t draft_offset;
  ActorConditionExecutionValues before;
  bool consumed = false;
};
FieldMilitaryAvailabilityPermit::FieldMilitaryAvailabilityPermit(
    CheckpointMemoryCharge memory, std::unique_ptr<Impl> impl)
    : memory_(std::move(memory)), impl_(std::move(impl)) {}
FieldMilitaryAvailabilityPermit::~FieldMilitaryAvailabilityPermit() = default;
Outcome<std::unique_ptr<FieldMilitaryAvailabilityPermit>>
prepare_field_military_availability(CommonStore &common,
                                    AuthoredActorFoundationState &state,
                                    const CharacterConditionSources &body,
                                    const SourceRef &ref, Integer ordinal,
                                    const ReadStamp &stamp,
                                    const ReadStampAuthority &authority,
                                    const DomainReadVersionSource &versions) {
  try {
    if (&body.common != &common || &body.state != &state)
      fail("FieldAvailabilitySameCandidateRequired", ref.id);
    const auto actual = field_source(body, ref, ordinal);
    const auto &actor_id = actual.actor_change->actor_id;
    take(authority.validate(stamp, common.state().identity,
                            ReadMode::phase_current, common, versions));
    bool actor_read = false;
    for (const auto &resource : stamp.resources())
      if (resource.resource.kind == ResourceKey::Kind::actor &&
          resource.resource.id == actor_id)
        actor_read = true;
    if (!actor_read ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  ref.id) == stamp.source_ids().end() ||
        take(compare_world_moments(stamp.span().to,
                                   *actual.applied_at)) ||
        take(compare_world_moments(body.actual_now,
                                   *actual.applied_at)))
      fail("FieldAvailabilityActualReadMismatch", ref.id);
    const auto *actor = common.state().actors.find(actor_id);
    if (!actor || !actor->alive)
      fail("FieldAvailabilityLivingActorRequired", actor_id);
    actor_history(body, actor_id);
    const auto cut = std::make_pair(position(common, ref), ordinal);
    for (const auto &row :
         state.character_condition_extensions.capacity_transitions.rows()) {
      if (same(row.second.cause_ref, ref, ordinal))
        fail("FieldAvailabilitySourceAlreadyApplied", ref.id);
      if (row.second.actor_id == actor_id && availability(row.second)) {
        const auto prior = cause_position(row.second.cause_ref);
        if (std::make_pair(position(common, *prior.source), prior.ordinal) >=
            cut)
          fail("FieldAvailabilitySourceAlreadyPassed", ref.id);
      }
    }
    auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("FieldAvailabilityCaptureRequired", actor_id);
    auto savepoint = take(common.actor_condition_savepoint_boundary());
    const auto offset = common.actor_condition_execution_drafts().size();
    if (offset >= static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
      fail("FieldAvailabilityCaptureSequenceOverflow", actor_id);


    auto memory = CheckpointMemoryCharge(0);
    auto impl = std::make_unique<FieldMilitaryAvailabilityPermit::Impl>(
        FieldMilitaryAvailabilityPermit::Impl{
            {},
            &common,
            &state,
            &body.catalog,
            {checkpoint_fresh_string_copy(ref.domain),
             checkpoint_fresh_string_copy(ref.kind),
             checkpoint_fresh_string_copy(ref.id)},
            ordinal,
            actual.actor_change,
            *actual.applied_at,
            std::move(capture),
            std::move(savepoint),
            offset,
            {actor->hp, actor->fatigue, actor->injury, actor->alive,
             actor->available},
            false});
    return {true,
            std::unique_ptr<FieldMilitaryAvailabilityPermit>(
                new FieldMilitaryAvailabilityPermit(std::move(memory),
                                                    std::move(impl))),
            {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldAvailabilityMemoryExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Outcome<FieldMilitaryAvailabilityBinding>
bind_field_military_availability(CommonStore &common,
                                 AuthoredActorFoundationState &state,
                                 const CharacterConditionSources &body,
                                 FieldMilitaryAvailabilityPermit &permit) {
  try {
    if (!permit.impl_)
      fail("FieldAvailabilityPermitRequired");
    auto &p = *permit.impl_;
    if (p.consumed || p.common != &common || p.foundation != &state ||
        &body.common != &common || &body.state != &state ||
        p.catalog != &body.catalog ||
        common.actor_condition_capture_identity() != p.capture)
      fail("FieldAvailabilityPermitCandidateMismatch");
    take(common.validate_actor_condition_savepoint_boundary(p.savepoint));
    const auto actual = field_source(body, p.source, p.ordinal);
    if (actual.actor_change != p.actual_actor ||
        take(compare_world_moments(*actual.applied_at,
                                   p.at)) ||
        take(compare_world_moments(body.actual_now, p.at)))
      fail("FieldAvailabilityPermitSourceChanged", p.source.id);
    const auto &drafts = common.actor_condition_execution_drafts();
    if (drafts.size() != p.draft_offset + 1)
      fail("FieldAvailabilityExactWriteRequired", p.source.id);
    const auto &draft = drafts[p.draft_offset];
    auto equal = [](const ActorConditionExecutionValues &a,
                    const ActorConditionExecutionValues &b) {
      return a.hp == b.hp && a.fatigue == b.fatigue && a.injury == b.injury &&
             a.alive == b.alive && a.available == b.available;
    };
    auto after = p.before;
    after.available = false;
    if (draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
        draft.sequence != static_cast<Integer>(p.draft_offset + 1) ||
        draft.actor_id != actual.actor_change->actor_id || !draft.before ||
        !equal(*draft.before, p.before) || !equal(draft.after, after) ||
        !draft.available_written || draft.hp_written || draft.fatigue_written ||
        draft.injury_written || draft.alive_written)
      fail("FieldAvailabilityCapturedWriteMismatch", p.source.id);
    auto &rows = state.character_condition_extensions.capacity_transitions;
    for (const auto &row : rows.rows())
      if (same(row.second.cause_ref, p.source, p.ordinal))
        fail("FieldAvailabilitySourceAlreadyApplied", p.source.id);
    auto id = take(common.next_owned_id("capacity"));


    const auto memory = CheckpointMemoryCharge(0);
    CapacityTransitionV2 row;
    row.transition_id = checkpoint_fresh_string_copy(id.id());
    row.actor_id = checkpoint_fresh_string_copy(actual.actor_change->actor_id);
    row.purpose = {
        checkpoint_fresh_text_copy("original_military_availability"), 1,
        checkpoint_fresh_string_copy(body.catalog.identity.content_digest)};
    row.from = p.before.available;
    row.to = false;
    row.effective_at.kind = ActorFoundationAt::Kind::exact_moment;
    row.effective_at.exact_moment = p.at;
    row.cause_ref = MilitaryAvailabilityCauseV2{
        {checkpoint_fresh_string_copy(p.source.domain),
         checkpoint_fresh_string_copy(p.source.kind),
         checkpoint_fresh_string_copy(p.source.id)},
        1,
        p.ordinal};
    validate(body, row);
    if (!rows.insert(id.id(), std::move(row)))
      fail("DuplicateCapacityTransition", id.id());
    const auto *stored = rows.find(id.id());
    if (!stored)
      fail("FieldAvailabilityStoredTransitionRequired", id.id());
    actor_history(body, stored->actor_id);
    p.consumed = true;
    return {true, {stored, p.capture, draft.sequence}, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldAvailabilityMemoryExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
} // namespace hanmo::resident
