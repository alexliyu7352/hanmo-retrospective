#include "character_availability_source_retention_refs.hpp"
#include "character_condition_read.hpp"
#include "condition_source_read.hpp"
#include "created_origin.hpp"
#include "military_availability.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "named_custody_availability_sources.hpp"
#include "resident/domains/family/life_age_exact_phase.hpp"
#include "resident/domains/military/military_model.hpp"
#include <type_traits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code) {
  throw Failure{common_failure(code).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
bool same(SourceRefBuildView a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool is(SourceRefBuildView r, std::string_view domain, std::string_view kind) {
  return r.domain == domain && r.kind == kind && !r.id.empty();
}
const FactRecord *header(const CommonStore &c, SourceRefBuildView r) {
  for (const auto &e : c.state().facts.rows())
    if (e.first == r.id) {
      if (e.second.id != e.first || e.second.domain != r.domain ||
          e.second.kind != r.kind)
        fail("AvailabilityRetentionActualHeadMismatch");
      return &e.second;
    }
  fail("AvailabilityRetentionActualHeadRequired");
}
void selected(const CharacterConditionSources &b) {
  if (b.catalog.identity.content_digest.empty() ||
      b.catalog.identity.content_digest !=
          b.common.state().metadata.content_digest ||
      b.catalog.scenario_id != b.common.state().metadata.scenario_id)
    fail("AvailabilityRetentionSameCatalogRequired");
  take(compare_world_moments(b.actual_now, b.actual_now));
}
CharacterInitialValuesV2 initial(const CharacterConditionSources &b,
                                 const StableId &actor) {
  return take(read_character_initial_values_v2(
      b.common, b.state, b.catalog, b.family_catalog, b.shared, actor,
      b.actual_now, b.born_sources));
}
template <class Owner, class Row>
CharacterAvailabilitySourceRetentionRef
result(CharacterAvailabilityProvenanceSlot slot, Integer schema,
       const Owner &owner, const StableId &key, const Row &row,
       const FactRecord *h, const char *domain, const char *kind,
       CharacterAvailabilitySourceReleaseCapability release =
           CharacterAvailabilitySourceReleaseCapability::
               individual_release_not_provided) {
  return {
      slot,
      {domain, kind, key},
      schema,
      CharacterAvailabilitySourceOwnerRef<Owner, Row>{&owner, &key, &row, h},
      release};
}
// 只认原 availability cause 本文；不可借 ConditionPurposeCapacity 的另一条
// release 规则。
void capacity_cause(const CharacterConditionSources &b, SourceRefBuildView r,
                    Integer schema) {
  bool seen = false;
  for (const auto &e :
       b.state.character_condition_extensions.capacity_transitions.rows()) {
    if (!is_original_availability_transition(e.second))
      continue;
    const auto actual =
        take(read_original_availability_cause_position(b, e.second));
    if (same(r, *actual.source)) {
      if (actual.schema != schema)
        fail("AvailabilityRetentionCauseSchemaMismatch");
      seen = true;
    }
  }
  if (!seen)
    fail("AvailabilityRetentionCapacityCauseNotActual");
}
void death_cause(const CharacterConditionSources &b, SourceRefBuildView r,
                 Integer schema) {
  if (!b.deaths || !b.life_sources)
    fail("AvailabilityRetentionLife4Required");
  bool seen = false;
  for (const auto &e : b.deaths->deaths.rows()) {
    if (!e.second.precise)
      fail("AvailabilityRetentionLife4Required");
    take(validate_family_death4(b.common, e.second, *b.life_sources,
                                b.actual_now, b.military_history_order));
    const auto actual = take(
        read_life_cause4(b.common, *b.life_sources, e.second.precise->cause,
                         &b.actual_now, b.military_history_order));
    if (same(r, actual.cause_ref)) {
      if (actual.source_schema != schema)
        fail("AvailabilityRetentionCauseSchemaMismatch");
      seen = true;
    }
  }
  if (!seen)
    fail("AvailabilityRetentionDeathCauseNotActual");
}
CharacterAvailabilitySourceRetentionRef
resolve(const CharacterAvailabilitySourceRetentionContext &x,
        CharacterAvailabilityProvenanceSlot slot, SourceRefBuildView r,
        Integer schema) {
  const auto &b = x.condition;
  selected(b);
  using Slot = CharacterAvailabilityProvenanceSlot;
  using Release = CharacterAvailabilitySourceReleaseCapability;
  if (slot == Slot::initial_condition_seed) {
    if (!is(r, "actor_foundation", "initial_condition_seed") ||
        schema != take(read_initial_condition_seed_schema(b.catalog)))
      fail("AvailabilityRetentionSeedSelectionRequired");
    for (const auto &e :
         b.state.actor_start_context.initial_condition_seeds.rows())
      if (e.first == r.id) {
        if (e.first != e.second.seed_key ||
            initial(b, e.second.actor_id).condition != &e.second)
          fail("AvailabilityRetentionSeedOwnerMismatch");
        return result(slot, schema, b.state, e.first, e.second, nullptr,
                      "actor_foundation", "initial_condition_seed",
                      Release::original_condition_owner_gate);
      }
    fail("AvailabilityRetentionSeedRequired");
  }
  if (slot == Slot::created_creation) {
    if (!is(r, "family", "actor_creation") || schema != 2)
      fail("AvailabilityRetentionCreationSelectionRequired");
    const auto *h = header(b.common, r);
    const auto actual = initial(b, h->id);
    if (!actual.origin ||
        actual.origin->origin_kind != CharacterOriginRecordV2::Kind::created ||
        actual.origin->actor_id != h->id)
      fail("AvailabilityRetentionCreatedOriginRequired");
    return result(slot, schema, b.common, h->id, *h, h, "family",
                  "actor_creation");
  }
  if (slot == Slot::capacity_transition) {
    if (!is(r, "actor_foundation", "capacity_transition") || schema != 2)
      fail("AvailabilityRetentionCapacitySelectionRequired");
    for (const auto &e :
         b.state.character_condition_extensions.capacity_transitions.rows())
      if (e.first == r.id) {
        if (e.first != e.second.transition_id ||
            !is_original_availability_transition(e.second))
          fail("AvailabilityRetentionOriginalCapacityRequired");
        take(read_original_availability_cause_position(b, e.second));
        return result(slot, schema, b.state, e.first, e.second, nullptr,
                      "actor_foundation", "capacity_transition");
      }
    fail("AvailabilityRetentionCapacityRequired");
  }
  if (slot == Slot::death) {
    if (!is(r, "family_life", "family_death") || schema != 4 || !b.deaths ||
        !b.life_sources)
      fail("AvailabilityRetentionLife4Required");
    for (const auto &e : b.deaths->deaths.rows())
      if (e.first == r.id) {
        if (!e.second.precise || e.second.precise->death_id != e.first)
          fail("AvailabilityRetentionDeathKeyMismatch");
        take(validate_family_death4(b.common, e.second, *b.life_sources,
                                    b.actual_now, b.military_history_order));
        return result(slot, schema, *b.deaths, e.first, e.second,
                      header(b.common, r), "family_life", "family_death");
      }
    fail("AvailabilityRetentionDeathRequired");
  }
  if (slot != Slot::capacity_cause && slot != Slot::death_cause)
    fail("AvailabilityRetentionEstablishedSlotRequired");
  if (slot == Slot::capacity_cause)
    capacity_cause(b, r, schema);
  else
    death_cause(b, r, schema);
  const auto *h = header(b.common, r);
  if (is(r, "military", "military_casualties")) {
    // 同一原MilitaryCasualtyFact支持已经验证的Field source1；不要求未选dynamic2。
    if (!b.military || (schema != 1 && schema != 2) ||
        (schema == 2 && !b.military_exact) ||
        (schema == 1 && (slot != Slot::capacity_cause || !b.injury_field ||
          !b.injury_field_clock || !b.injury_field_clock->bound_to(b.common, *b.military, *b.injury_field))))
      fail("AvailabilityRetentionOriginalMilitarySourceRequired");
    if (slot == Slot::death_cause &&
        (!b.life_sources || &b.life_sources->military != b.military ||
         b.life_sources->military_exact != b.military_exact))
      fail("AvailabilityRetentionMilitaryLifeOwnerMismatch");
    const MilitaryCasualtyFact *found = nullptr;
    for (const auto &row : b.military->casualty_facts)
      if (row.id == r.id) {
        if (found)
          fail("AvailabilityRetentionDuplicateMilitarySource");
        found = &row;
      }
    if (!found)
      fail("AvailabilityRetentionMilitarySourceRequired");
    return result(slot, schema, *b.military, found->id, *found, h, "military",
                  "military_casualties");
  }
  if (slot == Slot::capacity_cause &&
      is(r, "military", "military_named_custody_term")) {
    if (schema != 1 || !b.military || !b.named_custody_availability ||
        !b.named_custody_availability->bound_to(b.common, *b.military, b.state,
                                                b.catalog))
      fail("AvailabilityRetentionCustodyOwnerRequired");
    const NamedMilitaryCustodyTerm *found = nullptr;
    for (const auto &row : b.military->named_custody.terms)
      if (row.id == r.id) {
        if (found)
          fail("AvailabilityRetentionDuplicateCustodyTerm");
        found = &row;
      }
    if (found)
      return result(slot, schema, *b.military, found->id, *found, h, "military",
                    "military_named_custody_term");
    fail("AvailabilityRetentionCustodyTermRequired");
  }
  if (slot == Slot::capacity_cause &&
      is(r, "military", "military_named_custody_released")) {
    if (schema != 1 || !b.military || !b.named_custody_availability ||
        !b.named_custody_availability->bound_to(b.common, *b.military, b.state,
                                                b.catalog))
      fail("AvailabilityRetentionCustodyOwnerRequired");
    const NamedMilitaryCustodyRelease *found = nullptr;
    for (const auto &row : b.military->named_custody.closures)
      if (row.id == r.id) {
        if (found)
          fail("AvailabilityRetentionDuplicateCustodyRelease");
        found = &row;
      }
    if (found)
      return result(slot, schema, *b.military, found->id, *found, h, "military",
                    "military_named_custody_released");
    fail("AvailabilityRetentionCustodyReleaseRequired");
  }
  if (slot == Slot::death_cause && is(r, "family_life", "age_check")) {
    if (schema != 1 || !x.age || !x.actual_age_state || !x.actual_age_catalog ||
        !b.deaths || !b.life_sources || !b.life_sources->age)
      fail("AvailabilityRetentionAgeOwnerRequired");
    const auto a = x.age->bindings();
    if (&a.state != x.actual_age_state || &a.catalog != x.actual_age_catalog ||
        &a.sources.common != &b.common || &a.sources.foundation != &b.state ||
        &a.sources.foundation_catalog != &b.catalog || &a.deaths != b.deaths ||
        take(compare_world_moments(take(a.clock.read_current_moment()),
                                   b.actual_now)))
      fail("AvailabilityRetentionAgeOwnerMismatch");
    for (const auto &e : a.state.checks.rows())
      if (e.first == r.id) {
        if (e.first != e.second.id ||
            e.second.outcome != AgeCheckFact::Outcome::died)
          fail("AvailabilityRetentionAgeDiedSourceRequired");
        take(validate_age_check_fact(a.sources, a.catalog, a.state, e.second));
        return result(slot, schema, a.state, e.first, e.second, h,
                      "family_life", "age_check",
                      Release::original_age_check_owner_gate);
      }
    fail("AvailabilityRetentionAgeCheckRequired");
  }
  // Sea 无 concrete owner；未知/后继 source 必须另按真 owner 补，不取 schema1
  // 默认。
  fail("AvailabilityRetentionSourceOwnerNotRegistered");
}
} // namespace
Outcome<CharacterAvailabilitySourceRetentionRef>
resolve_character_availability_source_retention_ref(
    const CharacterAvailabilitySourceRetentionContext &x,
    CharacterAvailabilityProvenanceSlot slot, SourceRefBuildView source,
    Integer schema) {
  try {
    return {true, resolve(x, slot, source, schema), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_character_availability_source_retention_ref(
    const CharacterAvailabilitySourceRetentionContext &x,
    const CharacterAvailabilitySourceRetentionRef &ref) {
  auto actual = resolve_character_availability_source_retention_ref(
      x, ref.slot, ref.source, ref.schema);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  const auto same_borrow = [](std::string_view a, std::string_view b) {
    return a.data() == b.data() && a.size() == b.size();
  };
  bool matches = actual.value.identity.index() == ref.identity.index() &&
                 actual.value.release == ref.release &&
                 same_borrow(ref.source.domain, actual.value.source.domain) &&
                 same_borrow(ref.source.kind, actual.value.source.kind) &&
                 same_borrow(ref.source.id, actual.value.source.id);
  if (matches)
    std::visit(
        [&](const auto &row) {
          using T = std::decay_t<decltype(row)>;
          const auto *other = std::get_if<T>(&actual.value.identity);
          matches = other && row.owner == other->owner &&
                    row.key == other->key && row.row == other->row &&
                    row.header == other->header;
        },
        ref.identity);
  return matches
             ? common_success()
             : common_failure("AvailabilityRetentionBorrowedIdentityMismatch");
}
} // namespace hanmo::resident
