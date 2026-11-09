// 看管解除只恢复对应首次捕获的真实availability前驱，不治疗HP/伤病/疲劳。
#include "character_condition_read.hpp"
#include "named_custody_availability_internal.hpp"
#include "resident/domains/military/named_military_custody_history_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
#include <charconv>
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
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
template <class F> Status status(F fn) {
  try {
    fn();
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("NamedCustodyAvailabilityMemoryExhausted", {},
                          ErrorKind::internal);
  }
}
StableId copy(const StableId &s) { return checkpoint_fresh_string_copy(s); }
bool selected(const CatalogIdentity &id, std::string_view key,
              std::string_view version) {
  std::size_t n = 0;
  for (const auto &row : id.module_versions)
    if (row.first == key) {
      if (row.second != version)
        return false;
      ++n;
    }
  return n == 1;
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
SourceRef copied(const SourceRef &source) {
  return {copy(source.domain), copy(source.kind), copy(source.id)};
}
const CapacityTransitionV2 *capture_for(const CharacterConditionSources &body,
                                        const StableId &term) {
  const CapacityTransitionV2 *out = nullptr;
  for (const auto &entry :
       body.state.character_condition_extensions.capacity_transitions.rows())
    if (const auto *cause = std::get_if<NamedMilitaryCaptureAvailabilityCause>(
            &entry.second.cause_ref);
        cause && cause->term_source.id == term) {
      if (out)
        fail("DuplicateNamedCustodyCaptureTransition", term);
      out = &entry.second;
    }
  return out;
}
const CapacityTransitionV2 *last_for(const CharacterConditionSources &body,
                                     const StableId &actor) {
  const CapacityTransitionV2 *out = nullptr;
  for (const auto &entry :
       body.state.character_condition_extensions.capacity_transitions.rows())
    if (entry.second.actor_id == actor &&
        is_original_availability_transition(entry.second))
      out = &entry.second;
  return out;
}
// 真死亡存在时只借其原精确来源作截止，不能由今日dead推算历史，亦不能靠末尾false掩盖死后恢复。
void before_actual_death(const CharacterConditionSources &body,
                         const CapacityTransitionV2 &row, const StableId &actor,
                         const WorldMoment &at) {
  if (!body.deaths)
    return;
  const FamilyDeathRecord *death = nullptr;
  for (const auto &entry : body.deaths->deaths.rows())
    if (entry.second.actor_id == actor) {
      if (death)
        fail("ConflictingDeathSource", actor);
      death = &entry.second;
    }
  if (!death)
    return;
  if (!death->precise || !body.life_sources)
    fail("NamedCustodyAvailabilityExactDeathSourceRequired", actor);
  take(validate_family_death4(body.common, *death, *body.life_sources,
                              body.actual_now, body.military_history_order));
  const auto compared =
      take(compare_world_moments(at, death->precise->effective_at));
  if (compared > 0)
    fail("NamedCustodyAvailabilityAfterDeath", row.transition_id);
  const SourceRef *source = nullptr;
  if (const auto *capture =
          std::get_if<NamedMilitaryCaptureAvailabilityCause>(&row.cause_ref))
    source = &capture->term_source;
  else if (const auto *release =
               std::get_if<NamedMilitaryReleaseAvailabilityCause>(
                   &row.cause_ref))
    source = &release->release_source;
  if (!source)
    fail("NamedCustodyAvailabilityCauseRequired", row.transition_id);
  const StableId *cause_id = nullptr;
  if (const auto *military =
          std::get_if<MilitaryDeathCause4>(&death->precise->cause))
    cause_id = &military->casualty_fact_id;
  else if (const auto *age =
               std::get_if<NaturalAgeDeathCause4>(&death->precise->cause))
    cause_id = &age->age_check_fact_id;
  else
    cause_id = &std::get<EnvironmentalSeaDeathCause4>(death->precise->cause)
                    .sea_loss_result_id;
  const auto &order = body.common.state().facts.order();
  const auto source_at = std::find(order.begin(), order.end(), source->id);
  const auto death_at =
      std::find(order.begin(), order.end(), death->precise->death_id);
  const auto cause_at = std::find(order.begin(), order.end(), *cause_id);
  if (source_at == order.end() || death_at == order.end() ||
      cause_at == order.end() || source_at >= death_at ||
      (!compared && source_at >= cause_at))
    fail("NamedCustodyAvailabilityAfterDeath", row.transition_id);
}
void common_row(const CharacterConditionSources &body,
                const CapacityTransitionV2 &row, const StableId &actor,
                const WorldMoment &at) {
  if (row.actor_id != actor ||
      row.purpose.id != "original_named_custody_availability" ||
      row.purpose.version != 1 ||
      row.purpose.catalog_digest != body.catalog.identity.content_digest ||
      row.expected_recovery_at ||
      row.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      take(compare_world_moments(row.effective_at.exact_moment, at)) ||
      take(compare_world_moments(at, body.actual_now)) > 0 ||
      body.common.state().facts.find(row.transition_id))
    fail("NamedCustodyAvailabilityTransitionMismatch", row.transition_id);
  before_actual_death(body, row, actor, at);
}
void validate_row(const CharacterConditionSources &body,
                  const CapacityTransitionV2 &row) {
  take(validate_named_custody_availability_bindings(body));
  if (const auto *cause =
          std::get_if<NamedMilitaryCaptureAvailabilityCause>(&row.cause_ref)) {
    if (cause->source_schema != 1)
      fail("NamedCustodyAvailabilityOriginalSchemaRequired", row.transition_id);
    const auto *term =
        take(body.named_custody_availability->read_capture(cause->term_source));
    common_row(body, row, term->actor_id, term->adopted_at);
    if (row.to || term->capture_application_schema != 1 ||
        term->capture_actor_ordinal < 0 ||
        term->capture_application_source.domain != "military" ||
        term->capture_application_source.kind != "field_battle_segment")
      fail("NamedCustodyAvailabilityFieldApplicationRequired",
           row.transition_id);
  } else if (const auto *cause =
                 std::get_if<NamedMilitaryReleaseAvailabilityCause>(
                     &row.cause_ref)) {
    if (cause->source_schema != 1)
      fail("NamedCustodyAvailabilityOriginalSchemaRequired", row.transition_id);
    const auto *release = take(
        body.named_custody_availability->read_release(cause->release_source));
    common_row(body, row, release->actor_id, release->released_at);
    const auto *capture =
        body.state.character_condition_extensions.capacity_transitions.find(
            cause->capture_transition_id);
    const auto *original =
        capture ? std::get_if<NamedMilitaryCaptureAvailabilityCause>(
                      &capture->cause_ref)
                : nullptr;
    if (!capture || !original || capture->actor_id != row.actor_id ||
        original->term_source.id != release->term_id ||
        capture_for(body, release->term_id) != capture || row.from ||
        row.to != capture->from)
      fail("NamedCustodyAvailabilityCapturePredecessorRequired",
           row.transition_id);
    validate_row(body, *capture);
  } else
    fail("NamedCustodyAvailabilityCauseRequired", row.transition_id);
}
class Coverage final : public NamedCustodyAvailabilitySourceSink {
  const CharacterConditionSources &body_;

public:
  explicit Coverage(const CharacterConditionSources &body) : body_(body) {}
  Status capture(const NamedMilitaryCustodyTerm &term) override {
    return status([&] {
      const auto *row = capture_for(body_, term.id);
      if (!row)
        fail("NamedCustodyAvailabilityCaptureCoverageMismatch", term.id);
      validate_row(body_, *row);
    });
  }
  Status release(const NamedMilitaryCustodyRelease &release) override {
    return status([&] {
      const CapacityTransitionV2 *found = nullptr;
      for (const auto &entry : body_.state.character_condition_extensions
                                   .capacity_transitions.rows())
        if (const auto *cause =
                std::get_if<NamedMilitaryReleaseAvailabilityCause>(
                    &entry.second.cause_ref);
            cause && cause->release_source.id == release.id) {
          if (found)
            fail("DuplicateNamedCustodyReleaseTransition", release.id);
          found = &entry.second;
        }
      if (!found)
        fail("NamedCustodyAvailabilityReleaseCoverageMismatch", release.id);
      validate_row(body_, *found);
    });
  }
};

MilitaryAvailabilityApplication
application(const StableId &id,
            const std::shared_ptr<const ActorConditionCaptureIdentity> &capture,
            Integer sequence, bool prior) {


  MilitaryAvailabilityApplication out;
  out.payload_memory = CheckpointCopyLease::reserve();
  out.transition_id = copy(id);
  out.already_applied = prior;
  if (!prior) {
    out.actual_writes.reserve(1);
    out.actual_writes.push_back({copy(id), capture, sequence});
  }
  return out;
}
Outcome<MilitaryAvailabilityApplication>
apply(CommonStore &common, AuthoredActorFoundationState &state,
      const CharacterConditionSources &body, const SourceRef &source,
      const ReadStamp &stamp, const ReadStampAuthority &authority,
      const DomainReadVersionSource &versions, bool release) {
  try {
    if (&common != &body.common || &state != &body.state)
      fail("NamedCustodyAvailabilitySameCandidateRequired", source.id);
    take(validate_named_custody_availability_bindings(body));
    const NamedMilitaryCustodyTerm *term = nullptr;
    const NamedMilitaryCustodyRelease *closure = nullptr;
    if (release)
      closure = take(body.named_custody_availability->read_release(source));
    else
      term = take(body.named_custody_availability->read_capture(source));
    const auto &actor_id = release ? closure->actor_id : term->actor_id;
    const auto &at = release ? closure->released_at : term->adopted_at;
    const CapacityTransitionV2 *existing = nullptr;
    for (const auto &entry :
         state.character_condition_extensions.capacity_transitions.rows()) {
      const SourceRef *ref = nullptr;
      if (release) {
        if (const auto *c = std::get_if<NamedMilitaryReleaseAvailabilityCause>(
                &entry.second.cause_ref))
          ref = &c->release_source;
      } else if (const auto *c =
                     std::get_if<NamedMilitaryCaptureAvailabilityCause>(
                         &entry.second.cause_ref))
        ref = &c->term_source;
      if (ref && same(*ref, source)) {
        if (existing)
          fail("DuplicateNamedCustodyAvailabilityCause", source.id);
        existing = &entry.second;
      }
    }
    if (existing) {
      take(validate_military_availability_actor_history(body, actor_id));
      validate_row(body, *existing);
      return {true, application(existing->transition_id, {}, 0, true), {}};
    }
    take(authority.validate(stamp, common.state().identity,
                            ReadMode::phase_current, common, versions));
    bool actor_read = false;
    for (const auto &r : stamp.resources())
      if (r.resource.kind == ResourceKey::Kind::actor &&
          r.resource.id == actor_id)
        actor_read = true;
    if (!actor_read ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  source.id) == stamp.source_ids().end() ||
        take(compare_world_moments(stamp.span().to, at)) ||
        take(compare_world_moments(body.actual_now, at)))
      fail("NamedCustodyAvailabilityActualReadRequired", source.id);
    take(validate_military_availability_actor_history(body, actor_id));
    const auto *actor = common.actor(actor_id);
    if (!actor || !actor->alive)
      fail("NamedCustodyAvailabilityLivingActorRequired", actor_id);
    const auto actual_capture = common.actor_condition_capture_identity();
    if (!actual_capture)
      fail("NamedCustodyAvailabilityActualCaptureRequired", actor_id);
    const auto *prior = last_for(body, actor_id);
    const CapacityTransitionV2 *captured = nullptr;
    if (release) {
      captured = capture_for(body, closure->term_id);
      if (!captured || prior != captured || actor->available)
        fail("NamedCustodyAvailabilityInterleavedCause", source.id);
      validate_row(body, *captured);
    }
    const auto source_position =
        take(read_actual_common_fact_position(common, source));
    if (prior) {
      const auto evidence =
          take(read_original_availability_cause_position(body, *prior));
      if (take(read_actual_common_fact_position(common, *evidence.source)) >=
          source_position)
        fail("NamedCustodyAvailabilitySourceAlreadyPassed", source.id);
    }
    char ordinal[32];
    const auto end = std::to_chars(ordinal, ordinal + sizeof(ordinal),
                                   common.state().next_instance_id);
    if (end.ec != std::errc{})
      fail("NamedCustodyAvailabilityAllocatorRequired");


    if (release)
      (void)0;
    else (void)0;
    // 先覆盖私有transition的普通字段；原有序表仍在insert时取得自己的行租约。
    const auto memory = CheckpointMemoryCharge(0);
    CapacityTransitionV2 row;
    row.transition_id = take(common.next_id("capacity"));
    row.actor_id = copy(actor_id);
    row.purpose = {"original_named_custody_availability", 1,
                   copy(body.catalog.identity.content_digest)};
    row.from = actor->available;
    row.to = release ? captured->from : false;
    row.effective_at.exact_moment = at;
    if (release)
      row.cause_ref = NamedMilitaryReleaseAvailabilityCause{
          copied(source), 1, copy(captured->transition_id)};
    else
      row.cause_ref = NamedMilitaryCaptureAvailabilityCause{
          copied(source), 1,
          prior ? std::optional<StableId>{copy(prior->transition_id)}
                : std::nullopt};
    validate_row(body, row);
    const auto offset = common.actor_condition_execution_drafts().size();
    ActorMutation mutation;
    mutation.actor_id = copy(actor_id);
    mutation.available = row.to;
    take(common.apply_actor(mutation));
    const auto &drafts = common.actor_condition_execution_drafts();
    if (drafts.size() != offset + 1 || !drafts[offset].available_written ||
        !drafts[offset].before ||
        drafts[offset].before->available != row.from ||
        drafts[offset].after.available != row.to || drafts[offset].hp_written ||
        drafts[offset].fatigue_written || drafts[offset].injury_written ||
        drafts[offset].alive_written)
      fail("NamedCustodyAvailabilityActualWriteMismatch", actor_id);
    auto result = application(row.transition_id, actual_capture,
                              static_cast<Integer>(offset + 1), false);
    if (!state.character_condition_extensions.capacity_transitions.insert(
            row.transition_id, std::move(row)))
      fail("DuplicateNamedCustodyAvailabilityTransition", source.id);
    return {true, std::move(result), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("NamedCustodyAvailabilityMemoryExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
} // namespace
bool named_custody_availability_selected(
    const ActorFoundationCatalog &catalog) noexcept {
  return selected(catalog.identity, "character_condition_extensions", "6");
}
Status validate_named_custody_availability_bindings(
    const CharacterConditionSources &body) {
  return status([&] {
    if (!named_custody_availability_selected(body.catalog) ||
        !selected(body.catalog.identity, "world_timeline", "2") ||
        !selected(body.catalog.identity, "action_elapsed", "2") ||
        !selected(body.catalog.identity, "actor_start_context", "3") ||
        !selected(body.catalog.identity, "character_origin_records", "2") ||
        !selected(body.catalog.identity, "named_military_custody", "1") ||
        !body.military || !body.named_custody_availability ||
        !body.named_custody_availability->bound_to(body.common, *body.military,
                                                   body.state, body.catalog))
      fail("NamedCustodyAvailabilitySelectedSourcesRequired");
  });
}
Status validate_named_custody_availability_transition(
    const CharacterConditionSources &body, const CapacityTransitionV2 &row) {
  return status([&] { validate_row(body, row); });
}
Status validate_named_custody_availability_coverage(
    const CharacterConditionSources &body) {
  return status([&] {
    bool rows = false;
    for (const auto &entry :
         body.state.character_condition_extensions.capacity_transitions.rows())
      if (std::holds_alternative<NamedMilitaryCaptureAvailabilityCause>(
              entry.second.cause_ref) ||
          std::holds_alternative<NamedMilitaryReleaseAvailabilityCause>(
              entry.second.cause_ref)) {
        rows = true;
        validate_row(body, entry.second);
      }
    if (named_custody_availability_selected(body.catalog)) {
      take(validate_named_custody_availability_bindings(body));
      Coverage sink(body);
      take(body.named_custody_availability->visit_original_sources(sink));
    } else if (rows || body.named_custody_availability)
      fail("UnexpectedNamedCustodyAvailabilitySources");
  });
}
Outcome<MilitaryAvailabilityApplication> apply_verified_named_custody_capture(
    CommonStore &common, AuthoredActorFoundationState &state,
    const CharacterConditionSources &body, const SourceRef &source,
    const ReadStamp &stamp, const ReadStampAuthority &authority,
    const DomainReadVersionSource &versions) {
  return apply(common, state, body, source, stamp, authority, versions, false);
}
Outcome<MilitaryAvailabilityApplication> apply_verified_named_custody_release(
    CommonStore &common, AuthoredActorFoundationState &state,
    const CharacterConditionSources &body, const SourceRef &source,
    const ReadStamp &stamp, const ReadStampAuthority &authority,
    const DomainReadVersionSource &versions) {
  return apply(common, state, body, source, stamp, authority, versions, true);
}
} // namespace hanmo::resident
