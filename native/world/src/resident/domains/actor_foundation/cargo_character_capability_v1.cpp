#include "cargo_character_capability_v1.hpp"
#include "authored_origin.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
Status register_original_cargo_handling_work_purpose_v1(
    ActorFoundationCatalog &catalog) {
  std::size_t selected = 0;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "cargo_handling") {
      if (++selected != 1 || module.second != "1")
        return common_failure("CargoHandlingOriginalSelectionRequired", {},
                              ErrorKind::unsupported_capability);
    }
  if (selected != 1 || catalog.identity.content_digest.empty() ||
      !catalog.normal_work_efficiency_rule)
    return common_failure("CargoHandlingOriginalWorkRuleRequired", {},
                          ErrorKind::unsupported_capability);
  auto valid = validate_character_capability_catalog(catalog);
  if (!valid.ok)
    return valid;
  const auto &rule = catalog.normal_work_efficiency_rule->rule_ref;


  auto scratch = CheckpointMemoryCharge(0);
  CharacterPurposeBinding expected{
      {"cargo_handling_worker", 1, catalog.identity.content_digest},
      rule,
      CharacterPurposeBinding::Reducer::normal_work};
  if (const auto *prior =
          catalog.capability_purpose_bindings.find(expected.purpose_ref.id)) {
    if (prior->reducer != expected.reducer ||
        !actor_foundation_rule_equal(prior->purpose_ref,
                                     expected.purpose_ref) ||
        !actor_foundation_rule_equal(prior->rule_ref, expected.rule_ref))
      return common_failure("CargoHandlingOriginalWorkPurposeConflict");
    return common_success();
  }
  // 原 OrderedTable 在复制真实 binding 前执行其有限 copy-plan；不借 scratch
  // 充当表拥有权。
  if (!catalog.capability_purpose_bindings.insert(expected.purpose_ref.id,
                                                  expected))
    return common_failure("CargoHandlingOriginalWorkPurposeConflict");
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident
