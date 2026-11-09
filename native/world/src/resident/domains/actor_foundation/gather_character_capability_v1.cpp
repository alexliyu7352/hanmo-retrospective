#include "gather_character_capability_v1.hpp"
#include "authored_origin.hpp"
#include "battle_injury_catalog_v1.hpp"
namespace hanmo::resident {
Status
register_original_gather_work_purpose_v1(ActorFoundationCatalog &catalog) {
  bool sites = false;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "resource_sites") {
      if (sites || module.second != "1")
        return common_failure("GatherOriginalResourceSites1Required", {},
                              ErrorKind::unsupported_capability);
      sites = true;
    }
  if (!sites || !battle_injury_source1_selected(catalog.identity) ||
      catalog.identity.content_digest.empty() ||
      !catalog.normal_work_efficiency_rule)
    return common_failure("GatherOriginalConditionWorkRuleRequired", {},
                          ErrorKind::unsupported_capability);
  // The immutable author input must already supply the original rule and its
  // exact thresholds. This is not permission to manufacture normal-work data.
  auto valid = validate_character_capability_catalog(catalog);
  if (!valid.ok)
    return valid;
  CharacterPurposeBinding expected{
      {"economic_gather_worker", 1, catalog.identity.content_digest},
      catalog.normal_work_efficiency_rule->rule_ref,
      CharacterPurposeBinding::Reducer::normal_work};
  if (const auto *prior =
          catalog.capability_purpose_bindings.find(expected.purpose_ref.id)) {
    if (prior->reducer != expected.reducer ||
        !actor_foundation_rule_equal(prior->purpose_ref,
                                     expected.purpose_ref) ||
        !actor_foundation_rule_equal(prior->rule_ref, expected.rule_ref))
      return common_failure("GatherOriginalWorkPurposeConflict");
    return common_success();
  }
  const auto id = expected.purpose_ref.id;
  if (!catalog.capability_purpose_bindings.insert(id, std::move(expected)))
    return common_failure("GatherOriginalWorkPurposeConflict");
  // This finite id was already admitted by work_purpose and normal_work in the
  // original catalog validator. Its efficiency remains true source evidence;
  // the approved Gather quantity rule does not multiply harvest by this value.
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident
