// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
Outcome<std::shared_ptr<const PreparedCatalog>>
prepare_catalog_with_original_field(
    const BoundaryValue &catalog, const BoundaryValue &state,
    std::shared_ptr<const FieldBattleCatalog> original_field) {
  try {
    using namespace boundary;
    auto result = checkpoint_make_shared<PreparedCatalog>();
    // 原 native 工厂已核资源与 admitted；目录公开前原完整选型必须看见真实根。
    result->field_battle = std::move(original_field);
    const auto &data = field(catalog, "data", "catalog");
    if (!get_bool(catalog, "ok", "catalog"))
      fail("InvalidCatalog", "catalog");
    auto &id = result->identity;
    id.content_id = get_string(catalog, "content_id", "catalog");
    id.content_version = get_string(catalog, "content_version", "catalog");
    id.content_digest = get_string(catalog, "content_digest", "catalog");
    id.schema_sha256 = get_string(catalog, "schema_sha256", "catalog");
    id.ruleset_id = get_string(state, "ruleset_id", "");
    if (auto *pack = catalog.find("pack_set_digest"))
      id.pack_set_digest = string(*pack, "catalog/pack_set_digest");
    auto encoded = render_boundary_json(catalog);
    if (!encoded.ok)
      return {false, {}, encoded.error};
    id.artifact_sha256 = picosha2::hash256_hex_string(encoded.value);
    const auto &scenario =
        field(field(data, "scenarios", "data"),
              get_string(state, "scenario_id", ""), "scenarios");
    if (scenario.find("military_officer_assignments")) {
      auto unique = military_officer_codec::validate_json_unique_keys(catalog);
      if (!unique.ok)
        return {false, {}, unique.error};
    }
    NativeCatalogCapabilities required_capabilities(data);
#include "resident/domains/interaction/world_codec/catalog_message_profiles.inc"
#include "resident/domains/tasks/world_codec/catalog_offer_profiles.inc"
    auto selected_plan_version = decode_player_plan_profile_version(scenario);
    if (!selected_plan_version.ok)
      return {false, {}, selected_plan_version.error};
    result->player_plans = selected_plan_version.value != 0;
    if (const auto *dynamic = scenario.find("dynamic_world"))
      if (dynamic->find("regional_life")) {
        const auto &caps = required_capabilities();
        if (std::find(caps.begin(), caps.end(), "regional_life_v1") ==
            caps.end())
          fail("RegionalLifeCapabilityRequired", "dynamic_world/regional_life");
        result->regional_life = true;
      }
    if (const auto *dynamic = scenario.find("dynamic_world"))
      if (const auto *organization = dynamic->find("organization"))
        if (const auto *selected =
                organization->find("named_membership_enabled")) {
          result->named_membership = boolean(
              *selected, "dynamic_world/organization/named_membership_enabled");
          if (result->named_membership) {
            if (!result->regional_life)
              fail("NamedMembershipRegionalLifeRequired",
                   "dynamic_world/organization");
            const auto &caps = required_capabilities();
            if (std::find(caps.begin(), caps.end(), "named_membership_v1") ==
                caps.end())
              fail("NamedMembershipCapabilityRequired",
                   "dynamic_world/organization");
          }
        }
#include "resident/domains/tasks/world_codec/catalog_personal_advance.inc"
#include "resident/domains/family/world_codec/catalog_life.inc"
    auto a = decode_common_catalog(data);
    if (!a.ok)
      return {false, {}, a.error};
    if (const auto *initial = scenario.find("initial")) {
      auto roles = bind_native_authored_role_seed(a.value, *initial);
      if (!roles.ok)
        return {false, {}, std::move(roles.error)};
    }
    a.value.extended_skills = native_extended_skills(scenario);
    a.value.family_birth_identity_enabled =
        scenario.find("family_birth") &&
        scenario.find(
            "family_lineage"); // 本prepare私有候选保唯一可写别名，所有域只读引用同一CommonCatalog；发布return后别名消失。
    auto authored_common_candidate =
        checkpoint_make_shared<CommonCatalog>(std::move(a.value));
    result->common = authored_common_candidate;
#include "resident/domains/travel/world_codec/catalog_travel.inc"
#include "resident/domains/military/world_codec/catalog_original_naval.inc"
#include "resident/domains/interaction/world_codec/catalog_original.inc"
    // 仅从原作者主对话与岗位上下文连接补足私有目录；运行时许可/商铺严格门不变。
    auto role_dialogues =
        bind_native_role_dialogues_from_original_actor_dialogues(
            *authored_common_candidate, *result->interaction);
    if (!role_dialogues.ok)
      return {false, {}, std::move(role_dialogues.error)};

#include "resident/domains/social/world_codec/catalog_original.inc"
#include "resident/domains/estate/world_codec/catalog_original.inc"
#include "resident/domains/tasks/world_codec/catalog_original_extensions.inc"
#include "resident/domains/interaction/world_codec/catalog_appointments_inquiries.inc"
#include "resident/domains/tasks/world_codec/catalog_regional_sources.inc"
#include "resident/domains/family/world_codec/catalog_inheritance_birth.inc"
#include "resident/domains/interaction/world_codec/catalog_born_dialogues.inc"
#include "resident/domains/family/world_codec/catalog_care_legacy.inc"
#include "resident/domains/personal/world_codec/catalog_original.inc"
    // 本prepare独占可写别名，最终模块轴齐后绑定布局；发布后不保留可变访问。
    std::shared_ptr<GrowthMarketHistoryCatalog> authored_market_candidate;
#include "resident/domains/growth/world_codec/catalog_study_market.inc"
    if (const auto *dynamic = scenario.find("dynamic_world")) {
      if (get_string(*dynamic, "profile", "dynamic_world") !=
              "local_society_v1" ||
          id.ruleset_id != dynamic_ruleset_id)
        fail("UnsupportedNativeProfile", "dynamic_world");
      result->dynamic_profile = DynamicProfile::local_society_v1;
#include "resident/domains/local_economy/world_codec/catalog_supply_policies.inc"
#include "resident/domains/estate/world_codec/catalog_employment.inc"
#include "resident/domains/military/world_codec/catalog_population.inc"
#include "resident/domains/politics/world_codec/catalog_governance_fiscal.inc"
#include "resident/domains/local_economy/world_codec/catalog_relief_settlement.inc"
#include "resident/domains/tasks/world_codec/catalog_governance.inc"
#include "resident/domains/local_economy/world_codec/catalog_production_household.inc"
#include "resident/domains/organization/world_codec/catalog_runtime.inc"
#include "resident/domains/politics/world_codec/catalog_appointments_takeover.inc"
#include "resident/domains/local_economy/world_codec/catalog_occupation_fiscal.inc"
#include "resident/domains/politics/world_codec/catalog_diplomacy_passage.inc"
#include "resident/domains/affairs/world_codec/catalog_needs.inc"
#include "resident/domains/director/world_codec/catalog_subjects_intents.inc"
      id.module_versions = {
          {"dynamic_world", "local_society_v1"},
          {"local_economy", household_enabled(*result)    ? "5"
                            : management_enabled(*result) ? "4"
                            : result->named_membership    ? "3"
                            : population_enabled(*result) ? "2"
                                                          : "1"},
          {"organization", result->named_membership ? "3" : "2"},
          {"affairs", cohort_wage_needs_enabled(*result)  ? "4"
                      : fiscal_needs_enabled(*result)     ? "3"
                      : obligation_needs_enabled(*result) ? "2"
                                                          : "1"},
          {"director",
           result->director->subjects2
               ? (result->director->command_acceptance_bindings ? "6" : "5")
           : registry_enabled(*result) ? "4"
           : result->named_membership  ? "3"
           : result->regional_life     ? "2"
                                       : "1"}};
      if (population_enabled(*result)) {
        auto selected = selected_population_military_version(
            *result->population_military, siege_enabled(*result));
        if (!selected.ok)
          return {false, {}, selected.error};
        id.module_versions.emplace_back("population_military",
                                        std::to_string(selected.value));
      }
      if (result->regional_life)
        id.module_versions.emplace_back("regional_life", "1");
      if (siege_enabled(*result))
        id.module_versions.emplace_back("siege_integration", "1");
      if (result->named_membership)
        id.module_versions.emplace_back("named_membership", "1");
    } else if (id.ruleset_id == dynamic_ruleset_id)
      fail("MissingDynamicProfile", "scenario");
#include "resident/domains/commerce/world_codec/catalog_shop_legal_profiles.inc"
#include "resident/domains/family/world_codec/catalog_apprenticeship.inc"
#include "resident/domains/growth/world_codec/catalog_tactic_lessons.inc"
#include "resident/domains/interaction/world_codec/catalog_known_dialogues.inc"
#include "resident/domains/family/world_codec/catalog_voluntary_paths.inc"
#include "resident/domains/tasks/world_codec/catalog_personal_contracts.inc"
#include "resident/domains/family/world_codec/catalog_biological_collateral.inc"
#include "resident/checkpoint/world_codec/catalog_module_registration.inc"
#include "resident/domains/travel/world_codec/catalog_world_templates.inc"
#include "resident/checkpoint/world_codec/catalog_native_registration.inc"
#include "resident/checkpoint/world_codec/catalog_time_registration.inc"
    // 静态组织也只借最终已锁定作者身份；runtime 原绑定根不得重建。
    if (result->organization) {
      auto organization_runtime = selected_native_organization_runtime(id);
      if (!organization_runtime.ok)
        return {false, {}, std::move(organization_runtime.error)};
      if (!organization_runtime.value)
        result->organization =
            copy_locked_native_catalog(*result->organization, id);
    }
    if (authored_market_candidate) {
      auto market_layout = bind_native_growth_market_catalog(
          *result, *authored_market_candidate);
      if (!market_layout.ok)
        return {false, {}, std::move(market_layout.error)};
    }
    auto shop_catalog = validate_native_private_shop_operation_catalog(*result);
    if (!shop_catalog.ok)
      return {false, {}, std::move(shop_catalog.error)};
    auto work_catalog = validate_native_task_work_catalog(*result);
    if (!work_catalog.ok)
      return {false, {}, std::move(work_catalog.error)};
    auto social_selection = validate_native_social_catalog_selection(*result);
    if (!social_selection.ok)
      return {false, {}, std::move(social_selection.error)};
    auto funding_selection =
        validate_native_private_transport_funding_catalog(*result);
    if (!funding_selection.ok)
      return {false, {}, std::move(funding_selection.error)};
    auto production_selection =
        validate_native_production_cycles_catalog(*result);
    if (!production_selection.ok)
      return {false, {}, std::move(production_selection.error)};
    auto interaction_selection = validate_native_interaction_catalog(*result);
    if (!interaction_selection.ok)
      return {false, {}, std::move(interaction_selection.error)};
    auto transport_selection = validate_native_transport_catalog(*result);
    if (!transport_selection.ok)
      return {false, {}, std::move(transport_selection.error)};
    auto director_selection =
        validate_native_director_catalog_selection(*result);
    if (!director_selection.ok)
      return {false, {}, std::move(director_selection.error)};
    auto task_execution_catalog =
        prepare_native_task_execution_catalog(scenario, *result);
    if (!task_execution_catalog.ok)
      return {false, {}, std::move(task_execution_catalog.error)};
    auto personal_lifecycle_catalog =
        validate_native_personal_battle_lifecycle_catalog(*result);
    if (!personal_lifecycle_catalog.ok)
      return {false, {}, std::move(personal_lifecycle_catalog.error)};
    auto battle_lifecycle_catalog =
        validate_native_military_battle_lifecycle_catalog(*result);
    if (!battle_lifecycle_catalog.ok)
      return {false, {}, std::move(battle_lifecycle_catalog.error)};
    // 全部 module 与原目录验证完成后才冻结 Care 能力身份，不新增 module。
    auto care_selection = prepare_native_care_root_capability_selection(
        data, get_string(state, "scenario_id", ""), *result);
    if (!care_selection.ok)
      return {false, {}, std::move(care_selection.error)};
    return {true, std::move(result), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}

// 原公开入口不猜资源或构造占位目录；未选 Field 的原流程与全部拒门保持。
Outcome<std::shared_ptr<const PreparedCatalog>>
prepare_catalog(const BoundaryValue &catalog, const BoundaryValue &state) {
  return prepare_catalog_with_original_field(catalog, state, {});
}
