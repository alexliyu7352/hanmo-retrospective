// 只将已经存在的原身份/出生对象解析为同 World 有限拥有根身份。
#include "character_availability_retention_refs.hpp"
#include "character_condition_read.hpp"
#include "created_origin.hpp"
#include "resident/domains/family/born_character_source.hpp"
#include "resident/domains/family/family_birth.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <cstddef>
#include <type_traits>
#include <utility>
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
template <class F> Outcome<CharacterAvailabilityRetentionRef> guarded(F fn) {
  try {
    return {true, {fn()}, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
template <class T>
const StableId &key(const OrderedTable<T> &table, const T &value) {
  for (const auto &row : table.rows())
    if (&row.second == &value)
      return row.first;
  fail("CharacterAvailabilityRetentionOriginalRowRequired");
}
void selected(const CharacterAvailabilityRetentionContext &x) {
  const auto &s = x.condition;
  std::size_t count = 0;
  for (const auto &m : s.catalog.identity.module_versions)
    if (m.first == "actor_start_context") {
      if (++count != 1 || m.second != "3")
        fail("CharacterAvailabilityRetentionOrigin2SelectionRequired");
    }
  if (count != 1 || s.catalog.identity.content_digest.empty() ||
      s.catalog.identity.content_digest !=
          s.common.state().metadata.content_digest ||
      s.catalog.scenario_id != s.common.state().metadata.scenario_id)
    fail("CharacterAvailabilityRetentionCatalogMismatch");
  take(compare_world_moments(s.actual_now, s.actual_now));
}
const CharacterOriginRecordV2 &
original(const CharacterAvailabilityRetentionContext &x,
         const StableId &actor) {
  selected(x);
  const auto &s = x.condition;
  auto actual = take(read_character_initial_values_v2(
      s.common, s.state, s.catalog, s.family_catalog, s.shared, actor,
      s.actual_now, s.born_sources));
  const auto *row = s.state.character_origin_records.origins.find(actor);
  if (!row || row != actual.origin || row->actor_id != actor ||
      key(s.state.character_origin_records.origins, *row) != actor)
    fail("CharacterAvailabilityRetentionOriginOwnerMismatch", actor);
  return *row;
}
const BornCharacterSource &born_source(const CharacterOriginRecordV2 &origin) {
  const auto *source = std::get_if<BornCharacterSource>(&origin.source);
  if (origin.origin_kind != CharacterOriginRecordV2::Kind::born || !source)
    fail("CharacterAvailabilityRetentionBornOriginRequired", origin.actor_id);
  return *source;
}
struct BirthOwner {
  const StableId *key = nullptr;
  const FamilyBirthRecord *row = nullptr;
  const FactRecord *header = nullptr;
};
BirthOwner birth(const CharacterAvailabilityRetentionContext &x,
                 const BirthFactRef &ref, const StableId &actor) {
  const auto &s = x.condition;
  if (!x.births || !s.born_sources || !s.born_sources->bound_to(s.common))
    fail("CharacterAvailabilityRetentionBornOwnerRequired", ref.birth_fact_id);
  std::size_t count = 0;
  for (const auto &m : s.catalog.identity.module_versions)
    if (m.first == "family_birth") {
      if (++count != 1 || m.second != "4")
        fail("CharacterAvailabilityRetentionBirth4SelectionRequired",
             ref.birth_fact_id);
    }
  if (count != 1)
    fail("CharacterAvailabilityRetentionBirth4SelectionRequired",
         ref.birth_fact_id);
  // 原 reader 验原 Family/安排/随机及实际父源；这里只核真实根与头身份。
  const auto actual = take(s.born_sources->read_original_birth(ref));
  const auto *row = x.births->births.find(ref.birth_fact_id);
  const auto *header = s.common.state().facts.find(ref.birth_fact_id);
  if (!row || actual.birth != row || row->actor_id != actor ||
      row->kind != "family_birth_recorded" || !header ||
      header != actual.original_fact || header->id != ref.birth_fact_id ||
      header->domain != "family_birth" || header->kind != row->kind)
    fail("CharacterAvailabilityRetentionBirthOwnerMismatch", ref.birth_fact_id);
  take(read_actual_common_fact_position(s.common, *header));
  const auto &owned_key = key(x.births->births, *row);
  if (owned_key != ref.birth_fact_id)
    fail("CharacterAvailabilityRetentionBirthKeyMismatch", ref.birth_fact_id);
  return {&owned_key, row, header};
}
const CharacterOriginRecordV2 &
origin_for_birth_reference(const CharacterAvailabilityRetentionContext &x,
                           const BirthFactRef &ref) {
  // 回调原件来自 Origin2.source；相同字符串的外部 BirthFactRef 不算原借源。
  for (const auto &entry :
       x.condition.state.character_origin_records.origins.rows()) {
    const auto *source = std::get_if<BornCharacterSource>(&entry.second.source);
    if (source && &source->birth_record_ref == &ref) {
      const auto &actual = original(x, entry.first);
      if (&actual != &entry.second ||
          &born_source(actual).birth_record_ref != &ref)
        fail("CharacterAvailabilityRetentionBirthReferenceMismatch",
             entry.first);
      return actual;
    }
  }
  fail("CharacterAvailabilityRetentionOriginalBirthReferenceRequired",
       ref.birth_fact_id);
}
template <class T> bool same(const T &a, const T &b) {
  if constexpr (std::is_same_v<T, CharacterOriginOwnedRetentionRef> ||
                std::is_same_v<T, CharacterRegistrationOwnedRetentionRef> ||
                std::is_same_v<T, CharacterBirthAllocationOwnedRetentionRef>) {
    return a.owner == b.owner && a.key == b.key && a.row == b.row;
  } else if constexpr (std::is_same_v<
                           T, CharacterOriginalBirthOwnedRetentionRef>) {
    return a.owner == b.owner && a.key == b.key && a.row == b.row &&
           a.header == b.header && a.original_reference == b.original_reference;
  } else {
    return a.owner == b.owner && a.key == b.key && a.header == b.header &&
           a.birth_owner == b.birth_owner && a.birth_key == b.birth_key &&
           a.birth == b.birth;
  }
}
} // namespace
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &x,
    const CharacterOriginRecordV2 &row) {
  return guarded([&]() -> CharacterAvailabilityOwnedRetentionIdentity {
    const auto &actual = original(x, row.actor_id);
    if (&actual != &row)
      fail("CharacterAvailabilityRetentionOriginalOriginRequired",
           row.actor_id);
    const auto &owner = x.condition.state.character_origin_records;
    return CharacterOriginOwnedRetentionRef{&owner, &key(owner.origins, row),
                                            &row};
  });
}
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &x,
    const RegistrationSourceV2 &row) {
  return guarded([&]() -> CharacterAvailabilityOwnedRetentionIdentity {
    const auto &owner = x.condition.state.character_origin_records;
    const auto &owned_key = key(owner.registration_sources, row);
    if (owned_key != row.registration_result_id ||
        row.registration_kind != RegistrationSourceV2::Kind::created)
      fail("CharacterAvailabilityRetentionCreatedRegistrationRequired",
           owned_key);
    const auto &origin = original(x, row.actor_id);
    const auto *source = std::get_if<CreatedCharacterSource>(&origin.source);
    if (origin.origin_kind != CharacterOriginRecordV2::Kind::created ||
        !source || source->creation_result_ref != owned_key ||
        owner.registration_sources.find(source->creation_result_ref) != &row)
      fail("CharacterAvailabilityRetentionRegistrationOwnerMismatch",
           owned_key);
    return CharacterRegistrationOwnedRetentionRef{&owner, &owned_key, &row};
  });
}
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &x,
    const InitialCapabilityAllocation &row) {
  return guarded([&]() -> CharacterAvailabilityOwnedRetentionIdentity {
    const auto &owner = x.condition.state.character_origin_records;
    const auto &owned_key = key(owner.allocation_sources, row);
    if (owned_key != row.allocation_result_id ||
        row.allocation_kind != InitialCapabilityAllocation::Kind::birth)
      fail("CharacterAvailabilityRetentionBirthAllocationRequired", owned_key);
    const auto &origin = original(x, row.actor_id);
    const auto &source = born_source(origin);
    const auto *basis = std::get_if<BirthInitialCapabilityBasis>(
        &origin.initial_capability_basis);
    const auto *ref = std::get_if<BirthFactRef>(&row.original_source_ref);
    if (!basis || basis->allocation_source_ref != owned_key || !ref ||
        ref->birth_fact_id != source.birth_record_ref.birth_fact_id ||
        owner.allocation_sources.find(basis->allocation_source_ref) != &row)
      fail("CharacterAvailabilityRetentionAllocationOwnerMismatch", owned_key);
    const auto actual = birth(x, source.birth_record_ref, origin.actor_id);
    if (actual.row->allocation_source_ref != owned_key)
      fail("CharacterAvailabilityRetentionBirthAllocationLinkMismatch",
           owned_key);
    return CharacterBirthAllocationOwnedRetentionRef{&owner, &owned_key, &row};
  });
}
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &x, const BirthFactRef &ref) {
  return guarded([&]() -> CharacterAvailabilityOwnedRetentionIdentity {
    const auto &origin = origin_for_birth_reference(x, ref);
    const auto actual = birth(x, ref, origin.actor_id);
    return CharacterOriginalBirthOwnedRetentionRef{
        x.births, actual.key, actual.row, actual.header, &ref};
  });
}
Outcome<CharacterAvailabilityRetentionRef>
resolve_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &x,
    const FactRecord &creation) {
  return guarded([&]() -> CharacterAvailabilityOwnedRetentionIdentity {
    const auto &common = x.condition.common;
    if (creation.domain != "family_birth" ||
        creation.kind != "actor_creation" ||
        common.state().facts.find(creation.id) != &creation)
      fail("CharacterAvailabilityRetentionBornCreationRequired", creation.id);
    const auto &origin = original(x, creation.id);
    const auto &source = born_source(origin);
    const auto actual = birth(x, source.birth_record_ref, origin.actor_id);
    const auto at = take(read_actual_common_fact_position(common, creation));
    const auto born_at =
        take(read_actual_common_fact_position(common, *actual.header));
    if (born_at >= at)
      fail("CharacterAvailabilityRetentionBornCreationOrderMismatch",
           creation.id);
    return CharacterBornCreationOwnedRetentionRef{
        &common,    &key(common.state().facts, creation),
        &creation,  x.births,
        actual.key, actual.row};
  });
}
Status validate_character_availability_retention_ref(
    const CharacterAvailabilityRetentionContext &x,
    const CharacterAvailabilityRetentionRef &ref) {
  if (ref.lifetime != CharacterAvailabilityRetentionLifetime::
                          published_root_owned_no_individual_release)
    return common_failure("CharacterAvailabilityRetentionLifetimeRequired");
  try {
    return std::visit(
        [&](const auto &value) -> Status {
          using T = std::decay_t<decltype(value)>;
          Outcome<CharacterAvailabilityRetentionRef> actual;
          if constexpr (std::is_same_v<
                            T, CharacterOriginalBirthOwnedRetentionRef>) {
            if (!value.original_reference || value.owner != x.births ||
                !value.owner)
              fail("CharacterAvailabilityRetentionSameBirthOwnerRequired");
            actual = resolve_character_availability_retention_ref(
                x, *value.original_reference);
          } else if constexpr (std::is_same_v<
                                   T, CharacterBornCreationOwnedRetentionRef>) {
            if (!value.header || value.owner != &x.condition.common)
              fail("CharacterAvailabilityRetentionSameCommonOwnerRequired");
            actual =
                resolve_character_availability_retention_ref(x, *value.header);
          } else {
            if (!value.row ||
                value.owner != &x.condition.state.character_origin_records)
              fail("CharacterAvailabilityRetentionSameOriginOwnerRequired");
            actual =
                resolve_character_availability_retention_ref(x, *value.row);
          }
          const auto checked = take(std::move(actual));
          const auto *typed = std::get_if<T>(&checked.identity);
          return typed && same(value, *typed)
                     ? common_success()
                     : common_failure(
                           "CharacterAvailabilityRetentionIdentityMismatch");
        },
        ref.identity);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
