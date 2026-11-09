// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原情景中的精确教育选择只在最终目录锁后形成，不按全包能力并集或物理版猜选。
#include "resident/domains/family/care_root_capability_selection.hpp"
#include "resident/storage/common_codec.hpp"
#include "world_copy_plan.hpp"
#include <string_view>
#include <tuple>

namespace hanmo::resident {
namespace {
// 当前 exact Care 根只支持已明确承载的3/4/5；缺项是未选，重复和未来值严格拒。
Outcome<std::optional<Integer>>
selected_version(const CatalogIdentity &identity) {
  std::optional<Integer> selected;
  for (const auto &row : identity.module_versions) {
    if (row.first != "care_requirements")
      continue;
    if (selected)
      return {
          false, {}, common_failure("DuplicateCareRootModuleSelection").error};
    if (row.second == "3")
      selected = 3;
    else if (row.second == "4")
      selected = 4;
    else if (row.second == "5")
      selected = 5;
    else
      return {false,
              {},
              common_failure("CareRootExactVersion345Required", {},
                             ErrorKind::unsupported_capability)
                  .error};
  }
  return {true, selected, {}};
}
// 在原字典中核真实单一键，避免 find() 把重复作者项悄悄取第一项。
const BoundaryValue *unique_field(const BoundaryValue &object,
                                  std::string_view key, std::string_view path) {
  const BoundaryValue *found = nullptr;
  for (const auto &row : boundary::object(object, path))
    if (row.first == key) {
      if (found)
        boundary::fail_parts("DuplicateCareRootSelectionField",
                             {path, "/", key});
      found = &row.second;
    }
  return found;
}
// 原身份八项逐字相同，module 顺序也保留，不仅比较 content digest。
bool same_identity(const CatalogIdentity &a, const CatalogIdentity &b) {
  return std::tie(a.content_id, a.content_version, a.content_digest,
                  a.ruleset_id, a.schema_sha256, a.pack_set_digest,
                  a.module_versions, a.artifact_sha256) ==
         std::tie(b.content_id, b.content_version, b.content_digest,
                  b.ruleset_id, b.schema_sha256, b.pack_set_digest,
                  b.module_versions, b.artifact_sha256);
}
// 已选目录不得用空身份锁定；正式 prepare 仍负责其完整摘要真实性。
bool complete_identity(const CatalogIdentity &id) {
  return !id.content_id.empty() && !id.content_version.empty() &&
         !id.content_digest.empty() && !id.ruleset_id.empty() &&
         !id.schema_sha256.empty() && !id.artifact_sha256.empty() &&
         (!id.pack_set_digest || !id.pack_set_digest->empty());
}
} // namespace

Status
prepare_native_care_root_capability_selection(const BoundaryValue &data,
                                              const StableId &scenario_id,
                                              PreparedCatalog &catalog) {
  try {
    if (scenario_id.empty() || scenario_id.size() > 256)
      return common_failure("CareRootActualScenarioRequired");
    const auto *scenarios = unique_field(data, "scenarios", "data");
    if (!scenarios)
      return common_failure("CareRootActualScenarioRequired");
    const auto *scenario =
        unique_field(*scenarios, scenario_id, "data/scenarios");
    if (!scenario)
      return common_failure("CareRootActualScenarioRequired", scenario_id);
    const auto *care = unique_field(*scenario, "care_requirements", "scenario");
    const auto *education =
        unique_field(*scenario, "family_education", "scenario");
    auto version = selected_version(catalog.identity);
    if (!version.ok)
      return {false, {}, version.error};
    // 无 Care 的原世界不新增选择 owner；相关作者项绝不可静默落回 legacy。
    if (!version.value) {
      if (care || education)
        return common_failure("CareRootSelectedModuleRequired", {},
                              ErrorKind::unsupported_capability);
      if (catalog.care_root_capability_selection)
        return common_failure("UnexpectedCareRootCapabilitySelection");
      return common_success();
    }
    if (!care)
      return common_failure("CareRootSelectedScenarioRequired", scenario_id);
    boundary::fields(*care, {"profile", "birth_requirement_rule_id"}, {},
                     "scenario/care_requirements");
    const auto *care_profile =
        unique_field(*care, "profile", "scenario/care_requirements");
    const auto *birth_rule_value = unique_field(
        *care, "birth_requirement_rule_id", "scenario/care_requirements");
    if (boundary::string_ref(*care_profile,
                             {"scenario/care_requirements/profile"}) !=
        "care_actual_time_v2")
      return common_failure("CareAuthorProfileRequired", {},
                            ErrorKind::unsupported_capability);
    const auto &birth_rule = boundary::string_ref(
        *birth_rule_value,
        {"scenario/care_requirements/birth_requirement_rule_id"});
    if (birth_rule.empty() || birth_rule.size() > 256)
      return common_failure("InvalidCareBirthRuleSelection");
    auto mode = CareRootCapabilitySelection::Education::unselected;
    if (education) {
      boundary::fields(*education, {"profile"}, {},
                       "scenario/family_education");
      const auto *education_profile =
          unique_field(*education, "profile", "scenario/family_education");
      if (boundary::string_ref(*education_profile,
                               {"scenario/family_education/profile"}) !=
          "family_education_v1")
        return common_failure("FamilyEducationExactProfileRequired", {},
                              ErrorKind::unsupported_capability);
      const auto *caps = unique_field(data, "required_capabilities", "data");
      if (!caps)
        return common_failure("FamilyEducationCapabilityRequired", {},
                              ErrorKind::unsupported_capability);
      std::size_t count = 0;
      boundary::visit_string_rows(
          boundary::array(*caps, "data/required_capabilities"),
          "data/required_capabilities", [&](const StableId &cap) {
            if (cap == "family_education_v1")
              ++count;
          });
      if (count != 1)
        return common_failure(count ? "DuplicateFamilyEducationCapability"
                                    : "FamilyEducationCapabilityRequired",
                              {}, ErrorKind::unsupported_capability);
      mode = CareRootCapabilitySelection::Education::family_education_v1;
    }
    if (!complete_identity(catalog.identity))
      return common_failure("CareRootCompleteCatalogIdentityRequired");
    // 先预留真实普通字段，再深复制完整最终身份与情景；lease随此不可变对象存活。


    auto held = CheckpointCopyLease::reserve();
    CatalogIdentity identity(catalog.identity);
    StableId scenario_key(scenario_id);
    CareRootCapabilitySelection value(std::move(held), std::move(identity),
                                      std::move(scenario_key), mode);
    auto owner = checkpoint_make_shared<const CareRootCapabilitySelection>(
        std::move(value));
    // native prepare 的最终 admitted 可以替换中间目录锁；失败前不破坏原 owner。
    catalog.care_root_capability_selection = std::move(owner);
    return common_success();
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}

Outcome<CareRootSelectedCapabilities>
read_native_care_root_capability_selection(const PreparedCatalog &catalog,
                                           const StableId &actual_scenario_id) {
  auto version = selected_version(catalog.identity);
  if (!version.ok)
    return {false, {}, version.error};
  const auto &owner = catalog.care_root_capability_selection;
  if (!version.value) {
    if (owner)
      return {false,
              {},
              common_failure("UnexpectedCareRootCapabilitySelection").error};
    return {true, {}, {}};
  }
  if (!owner)
    return {false,
            {},
            common_failure("CareRootCapabilitySelectionRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  if (!complete_identity(catalog.identity) ||
      !same_identity(owner->catalog_identity(), catalog.identity))
    return {
        false, {}, common_failure("CareRootCapabilityCatalogMismatch").error};
  if (actual_scenario_id.empty() || owner->scenario_id() != actual_scenario_id)
    return {
        false, {}, common_failure("CareRootCapabilityScenarioMismatch").error};
  return {true, {version.value, owner->family_education_selected()}, {}};
}
} // namespace hanmo::resident
