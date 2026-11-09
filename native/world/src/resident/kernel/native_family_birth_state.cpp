#include "resident/kernel/catalog_section_observation.hpp"
#include "native_character_injury_state.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "native_family_birth_state.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "original_catalog_identity.hpp"
#include "world_copy_plan.hpp"
#include "resident/domains/family/dependent_provision_profile.hpp"
#include "resident/domains/travel/birth_placement.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
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
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
[[noreturn]] void fail(const char *code) {
  throw Failure{
      common_failure(code, "family_birth", ErrorKind::unsupported_capability)
          .error};
}
const BoundaryValue *unique_field(const BoundaryValue &value, const char *key) {
  const BoundaryValue *out = nullptr;
  for (const auto &row : boundary::object(value, "birth_selection"))
    if (row.first == key) {
      if (out)
        fail("DuplicateFamilyBirthSelection");
      out = &row.second;
    }
  return out;
}
bool module(const CatalogIdentity &identity, const char *name,
            const char *version) {
  Integer count = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == name) {
      if (row.second != version)
        return false;
      ++count;
    }
  return count == 1;
}
} // namespace
Outcome<Integer>
selected_native_family_birth_version(const CatalogIdentity &identity) {
  try {
    Integer version = 0, lineage = 0;
    for (const auto &row : identity.module_versions) {
      if (row.first == "family_birth") {
        if (version || (row.second != "1" && row.second != "4"))
          fail("UnsupportedFamilyBirthVersion");
        version = row.second == "4" ? 4 : 1;
      } else if (row.first == "family_lineage") {
        if (lineage || row.second != "1")
          fail("UnsupportedFamilyLineageVersion");
        lineage = 1;
      }
    }
    if (bool(version) != bool(lineage))
      fail("FamilyBirthProfileMismatch");
    auto injury = selected_native_character_injury_source_version(identity);
    if (!injury.ok)
      return {false, 0, std::move(injury.error)};
    if (version == 4 &&
        (!module(identity, "actor_start_context", "3") ||
         !module(identity, "family_life", "4") ||
         !module(identity, "character_origin_records", "1") ||
         (!module(identity, "character_condition_extensions", "1") &&
          !module(identity, "character_condition_extensions", "7") &&
          !(injury.value == 1 &&
            module(identity, "character_condition_extensions", "8")))))
      fail("Birth4CompleteOriginalModulesRequired");
    return {true, version, {}};
  } catch (Failure &e) {
    return {false, 0, std::move(e.error)};
  }
}
Outcome<Integer>
read_native_family_birth_selection(const BoundaryValue &scenario) {
  try {
    const auto *birth = unique_field(scenario, "family_birth");
    const auto *lineage = unique_field(scenario, "family_lineage");
    if (bool(birth) != bool(lineage))
      fail("FamilyBirthProfileMismatch");
    if (!birth)
      return {true, 0, {}};
    const auto version = boundary::get_int(*birth, "version", "family_birth");
    const auto &profile =
        boundary::get_string_ref(*birth, "profile", "family_birth");
    if ((version != 1 && version != 4) ||
        (version == 1 && profile != "adult_birth_personal_care_v1") ||
        (version == 4 && profile != "birth_due_at_actual_location_sources_v4"))
      fail("UnsupportedFamilyBirthVersion");
    return {true, version, {}};
  } catch (Failure &e) {
    return {false, 0, std::move(e.error)};
  } catch (const boundary::DecodeFailure &e) {
    return {false, 0, e.error};
  }
}
Outcome<BirthArrangementCatalog>
decode_native_family_birth_catalog(const BoundaryValue &scenario,
                                   const CatalogIdentity &identity) {
  try {
    const auto version = take(read_native_family_birth_selection(scenario));
    const auto *value = unique_field(scenario, "family_birth");
    if (!version || !value)
      fail("FamilyBirthProfileRequired");
    return version == 1
               ? decode_birth_arrangement_catalog(*value)
               : decode_birth_arrangement_catalog_complete(*value, 4, identity);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status validate_native_family_birth_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 134, "PreparedCatalog", "validate_native_family_birth_catalog", &catalog);

  try {
    const auto version =
        take(selected_native_family_birth_version(catalog.identity));
    const bool enabled = catalog.family_birth && catalog.family_birth->enabled;
    if (bool(version) != enabled ||
        bool(version) != bool(catalog.family_lineage) ||
        (enabled && catalog.family_birth->selected_version != version))
      fail("FamilyBirthSelectedCatalogMismatch");
    if (version != 4) {
      if (catalog.dependent_provision_catalog)
        fail("DependentProvisionBirth4Required");
      return common_success();
    }
    if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
        !catalog.common || !catalog.family || !catalog.family->enabled ||
        !catalog.interaction ||
        catalog.interaction->common.get() != catalog.common.get() ||
        !catalog.actor_foundation || !catalog.family_lineage->enabled ||
        !catalog.family_life || !catalog.family_life->enabled ||
        catalog.family_life->selected_version != 4 ||
        !catalog.dependent_provision_catalog)
      fail("Birth4CompleteOriginalCatalogsRequired");
    const auto &birth = *catalog.family_birth;
    const auto &provision = *catalog.dependent_provision_catalog;
    if (!birth.due_policy_ref ||
        birth.due_policy_ref->catalog_digest !=
            catalog.identity.content_digest ||
        !birth.dependent_provision_profile_ref ||
        *birth.dependent_provision_profile_ref != provision.birth_profile_id ||
        !same_original_catalog_identity(provision.identity, catalog.identity))
      fail("DependentProvisionBirthCatalogMismatch");
    take(validate_birth_arrangement_catalog(
        birth, *catalog.common, *catalog.interaction, *catalog.family));
    take(validate_dependent_provision_catalog(provision));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status prepare_native_dependent_provision_catalog(const BoundaryValue &data,
                                                  PreparedCatalog &catalog) {
  try {
    const auto version =
        take(selected_native_family_birth_version(catalog.identity));
    if (catalog.dependent_provision_catalog)
      fail("DuplicateDependentProvisionCatalog");
    if (version != 4)
      return validate_native_family_birth_catalog(catalog);
    if (!catalog.family_birth ||
        !catalog.family_birth->dependent_provision_profile_ref)
      fail("DependentProvisionBirth4Required");
    const auto *definitions =
        unique_field(data, "dependent_provision_profiles");
    if (!definitions)
      fail("SelectedDependentProvisionProfileRequired");
    ScopedCheckpointJsonDecode original(
        CheckpointCatalogJsonKind::dependent_provision_catalog);
    original.default_values<DependentProvisionCatalog>();


    original.ordinary(std::size_t{0});
    auto decoded = take(decode_dependent_provision_catalog(
        *definitions, *catalog.family_birth->dependent_provision_profile_ref,
        catalog.identity));
    catalog.dependent_provision_catalog =
        checkpoint_make_shared<const DependentProvisionCatalog>(
            std::move(decoded));
    return validate_native_family_birth_catalog(catalog);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status validate_native_family_birth_layout(const WorldBundle &w) {
  try {
    if (!w.catalog)
      fail("Birth4CatalogRequired");
    take(validate_native_family_birth_catalog(*w.catalog));
    const auto version =
        take(selected_native_family_birth_version(w.catalog->identity));
    if (bool(version) != w.family_birth_seed_complete)
      fail("FamilyBirthSelectedRootMismatch");
    if (version == 4 && (!w.actor_foundation || !w.world_timeline ||
                         !w.action_elapsed || !w.family_life_seed_complete))
      fail("Birth4CompleteOriginalRootsRequired");
    if (version != 4 && !w.family_birth.birth_due_executions.empty())
      fail("BirthDueProfileRequired");
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
validate_native_family_birth_bindings(const WorldBundle &w,
                                      const BornCharacterSourceContext &x) {
  try {
    take(validate_native_family_birth_layout(w));
    if (take(selected_native_family_birth_version(w.catalog->identity)) != 4)
      fail("Birth4OriginalSourcesRequired");
    const auto &a = x.access;
    if (&x.common != &w.common ||
        &x.common_catalog != w.catalog->common.get() ||
        &x.family_catalog != w.catalog->family.get() ||
        &x.lineage != w.catalog->family_lineage.get() ||
        &x.interactions != w.catalog->interaction.get() ||
        &x.catalog != w.catalog->family_birth.get() ||
        &x.births != &w.family_birth ||
        a.lineage != w.catalog->family_lineage.get() ||
        a.life != &w.family_life || a.foundation != &*w.actor_foundation ||
        a.foundation_catalog != w.catalog->actor_foundation.get() ||
        !a.people || !a.people->bound_to(w.common) || !a.placement ||
        !a.placement->bound_to(w.common) || !a.born_sources ||
        !a.born_sources->bound_to(w.common, w.family_birth))
      fail("Birth4OriginalSourceRootsMismatch");
    const auto actual = take(w.actual_phase_moment()),
               now = take(a.people->read_current_moment());
    if (take(compare_world_moments(actual, now)) != 0)
      fail("Birth4OriginalSourceMomentMismatch");
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<FamilyBirthState> decode_native_family_birth(const BoundaryValue &value,
                                                     Integer version) {
  if (version == 1)
    return decode_family_birth(value);
  if (version == 4)
    return decode_family_birth_complete(value, 4);
  return {false, {}, common_failure("UnsupportedFamilyBirthVersion").error};
}
Status visit_native_family_birth(CodecVisitor &c, FamilyBirthState &value,
                                 Integer version) {
  if (version == 1)
    return visit_family_birth_checkpoint(c, value);
  if (version == 4)
    return visit_family_birth_checkpoint_complete(c, value, 4);
  return common_failure("UnsupportedFamilyBirthVersion");
}
} // namespace hanmo::resident
