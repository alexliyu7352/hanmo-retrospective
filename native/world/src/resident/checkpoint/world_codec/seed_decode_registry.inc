// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
namespace {
Status decode_world_seed_into(const BoundaryValue &source,
                              const PreparedCatalog &catalog,
                              InitialWorldSeed &result,
                              CodecDecodeBudget *owned_budget,
                              CheckpointJobBudget *owned_job) {
  try {
    // 私有 owned 路只借同一 budget/job；缺一不得静默退回裸解码。
    if (bool(owned_budget) != bool(owned_job))
      return common_failure("CheckpointJsonOwnedContextRequired",
                            "growth_terminal_publications",
                            ErrorKind::internal);
    if (catalog.growth &&
        catalog.growth->complete_capability_application_chain &&
        !source.find("trade-grant")) {
      auto error =
          common_failure("GrowthFormatVersionUnsupported", "trade-grant",
                         ErrorKind::unsupported_capability)
              .error;
      error.domain = "trade-grant";
      error.reason = "observed=missing, expected=1; no migration";
      return {false, {}, std::move(error)};
    }
    auto inspection_catalog =
        validate_native_public_supply_inspection_catalog(catalog);
    if (!inspection_catalog.ok)
      return inspection_catalog;
    if (catalog.public_supply_inspection) {
      auto decoded = [&] {
        const auto &original =
            field(source, "public_supply_inspection", "state");
        if (checkpoint_boundary_decode_active()) {
          ScopedCheckpointJsonDecode section(
              CheckpointWorldJsonSection::public_supply_inspection);
          return decode_public_supply_inspection_state(original, section);
        }
        return decode_public_supply_inspection_state(original);
      }();
      if (!decoded.ok)
        return {false, {}, std::move(decoded.error)};
      result.public_supply_inspection =
          checkpoint_make_shared<const PublicSupplyInspectionState>(
              std::move(decoded.value));
    } else if (source.find("public_supply_inspection"))
      return common_failure("UnexpectedPublicSupplyInspectionState");
    auto field_selection = validate_field_battle_selection(
        catalog.identity, catalog.field_battle.get());
    if (!field_selection.ok)
      return field_selection;
    if (catalog.field_group_passage) {
      auto decisions = checkpoint_make_shared<PoliticalPassageDecisionState>();
      auto safe_exit = checkpoint_make_shared<SafeExitState>();
      std::optional<ScopedCheckpointJsonDecode> section;
      if (owned_budget && owned_job)
        section.emplace(*owned_budget, *owned_job, "political_passage", current_checkpoint_memory());
      auto decoded = decode_field_passage_checkpoint(field(source, "political_passage", "state"), *decisions, *safe_exit);
      if (!decoded.ok) return decoded;
      result.political_passage = std::move(decisions);
      result.political_safe_exit = std::move(safe_exit);
    } else if (source.find("political_passage"))
      return common_failure("UnexpectedPoliticalPassageState", "political_passage");
    if (catalog.political_relations) {
      auto decoded = [&] {
        std::optional<ScopedCheckpointJsonDecode> section;
        if (owned_budget && owned_job)
          section.emplace(*owned_budget, *owned_job, "political_relations",
                          current_checkpoint_memory());
        return decode_political_relations(
            field(source, "political_relations", "state"));
      }();
      if (!decoded.ok)
        return {false, {}, std::move(decoded.error)};
      result.political_relations =
          checkpoint_make_shared<const PoliticalRelationsState>(
              std::move(decoded.value));
    } else if (source.find("political_relations"))
      return common_failure("UnexpectedPoliticalRelationsState",
                            "political_relations");
    if (catalog.field_battle) {
      auto decoded = [&] {
        if (checkpoint_boundary_decode_active()) {
          ScopedCheckpointJsonDecode section(
              CheckpointWorldJsonSection::field_battle);
          return decode_field_battle_world(
              field(source, "field_battle", "state"));
        }
        return decode_field_battle_world(
            field(source, "field_battle", "state"));
      }();
      if (!decoded.ok)
        return {false, {}, decoded.error};
      result.field_battle = checkpoint_make_shared<const FieldBattleWorldState>(
          std::move(decoded.value));
    } else if (source.find("field_battle"))
      return common_failure("UnexpectedFieldBattleState", "field_battle");
    auto delivery_version = profile_delivery_selected_version(catalog.identity);
    if (!delivery_version.ok)
      return {false, {}, std::move(delivery_version.error)};
    if (!delivery_version.value && source.find("profile_deliveries"))
      return {
          false,
          {},
          common_failure("UnexpectedProfileDeliveries", "profile_deliveries")
              .error};
    if (officers_enabled(catalog)) {
      auto unique = military_officer_codec::validate_json_unique_keys(source);
      if (!unique.ok)
        return {false, {}, unique.error};
    }
    if (family_life_enabled(catalog)) {
      auto unique = validate_succession_json_unique(source);
      if (!unique.ok)
        return {false, {}, unique.error};
    }
    if (catalog.economic_supply_policies_enabled ||
        catalog.growth_market_history || catalog.private_shops) {
      auto unique = validate_economic_payload_fields(source);
      if (!unique.ok)
        return {false, {}, unique.error};
    }
    ResidentResultCodec receipts;
#include "resident/domains/organization/world_codec/seed_runtime_selection.inc"
#include "resident/domains/local_economy/world_codec/seed_obligation_selection.inc"
#include "resident/domains/local_economy/world_codec/seed_obligation_sources.inc"
#include "resident/domains/local_economy/world_codec/seed_governance_programs.inc"
    if (!organization_runtime.value && source.find("organization_runtime"))
      fail("UnexpectedOrganizationRuntime", "organization_runtime");
    if (!organization_effects.value &&
        source.find("organization_service_effects"))
      fail("UnexpectedOrganizationServiceEffects",
           "organization_service_effects");
#include "resident/domains/craft/world_codec/seed_item_craft.inc"
#include "resident/domains/local_economy/world_codec/seed_facility_roots.inc"
#include "resident/domains/military/world_codec/seed_exact_actions.inc"
#include "resident/domains/commerce/world_codec/seed_shop_operations.inc"
#include "resident/domains/local_economy/world_codec/seed_private_shop_custody.inc"
    if (delivery_version.value == 1) {
      auto parsed = decode_profile_delivery_outbox(
          field(source, "profile_deliveries", "state"));
      if (!parsed.ok)
        return {false, {}, std::move(parsed.error)};
      result.profile_deliveries =
          checkpoint_make_shared<const ProfileDeliveryOutbox>(
              std::move(parsed.value));
    }
#include "resident/domains/local_economy/world_codec/seed_production_cycles.inc"
#include "resident/domains/commerce/world_codec/seed_transport_funding.inc"
#include "resident/domains/family/world_codec/seed_age_control.inc"
#include "resident/domains/growth/world_codec/seed_permanent.inc"
#include "resident/domains/local_economy/world_codec/seed_household_purchases.inc"
    // 人口段只由同一锁定P02/3选择解码；冷恢复不补造、未知残留仍拒绝。
    auto civilian_population_version =
        selected_native_population_transport_version(catalog.identity);
    if (!civilian_population_version.ok)
      return {false, {}, std::move(civilian_population_version.error)};
    if (civilian_population_version.value) {
      ScopedCheckpointJsonDecode original_decode(
          CheckpointWorldJsonSection::civilian_transport_population);
      auto population = decode_population_transport(
          field(source, "civilian_transport_population", "state"),
          bool(catalog.household_purchases), civilian_population_version.value);
      if (!population.ok)
        return {false, {}, std::move(population.error)};
      result.population_transport =
          checkpoint_make_shared<const PopulationTransportState>(
              std::move(population.value));
    } else if (source.find("civilian_transport_population"))
      fail("UnexpectedPopulationTransportState",
           "civilian_transport_population");
#include "resident/domains/actor_foundation/world_codec/seed_exact_origins.inc"
    auto c = decode_common_seed(source, *catalog.common, &receipts,
                                CommonSeedValidation::defer_until_bundle);
    if (!c.ok)
      return {false, {}, c.error};
    result.common = std::move(c.value);
    {
      CommonStore original(result.common, catalog.common);
      auto actions = validate_native_item_craft_original_actions(
          original, catalog, result.item_instances.get(),
          result.craft_jobs.get());
      if (!actions.ok)
        return {false, {}, std::move(actions.error)};
      for (const auto &row : result.common.actions.rows())
        if (row.second.kind == "craft_job" || row.second.kind == "repair")
          required_empty(
              field(field(source, "actions", "state"), row.first, "actions"),
              "params");
      auto payloads = projection_value([&](auto &out) {
        return emit_native_item_craft_fact_payloads_fields(
            out, original, catalog, result.item_instances.get(),
            result.craft_jobs.get(), NativeItemCraftFactProjection::all);
      });
      if (!payloads.ok)
        return {false, {}, std::move(payloads.error)};
      for (const auto &row : object(payloads.value, "item_craft/facts")) {
        const auto &actual =
            field(field(source, "facts", "state"), row.first, "facts");
        if (!compare_boundary(actual, row.second).exact())
          fail("ItemCraftOriginalFactPayloadMismatch", row.first);
      }
    }
    for (const auto &row : result.common.facts.rows())
      if (is_native_production_cycle_fact(row.second)) {
        if (!result.production_cycles)
          fail("ProductionCyclesFactStateRequired", row.first);
        auto checked = validate_native_production_cycle_fact(
            *result.production_cycles, row.second);
        if (!checked.ok)
          return {false, {}, std::move(checked.error)};
        fields(field(field(source, "facts", "state"), row.first, "facts"),
               {"kind", "domain"}, {}, row.first);
      }
    if (result.common.metadata.content_id != catalog.identity.content_id ||
        result.common.metadata.content_version !=
            catalog.identity.content_version ||
        result.common.metadata.content_digest !=
            catalog.identity.content_digest)
      return {false, {}, common_failure("ContentMismatch").error};
#include "resident/domains/travel/world_codec/seed_transport_registration.inc"
#include "resident/domains/military/world_codec/seed_original_lifecycle.inc"
#include "resident/domains/interaction/world_codec/seed_dialogues_appointments.inc"
#include "resident/domains/social/world_codec/seed_original.inc"
#include "resident/domains/estate/world_codec/seed_employment.inc"
#include "resident/domains/local_economy/world_codec/seed_supply_policies.inc"
#include "resident/domains/estate/world_codec/seed_original.inc"
#include "resident/domains/tasks/world_codec/seed_original_execution.inc"
#include "resident/domains/family/world_codec/seed_lifecycle.inc"
#include "resident/domains/growth/world_codec/seed_recovery_lessons.inc"
#include "resident/domains/personal/world_codec/seed_original_lifecycle.inc"
#include "resident/domains/growth/world_codec/seed_original.inc"
    if (catalog.dynamic_profile == DynamicProfile::local_society_v1) {
#include "resident/domains/local_economy/world_codec/seed_dynamic_economy.inc"
#include "resident/domains/organization/world_codec/seed_dynamic_runtime.inc"
#include "resident/domains/affairs/world_codec/seed_original.inc"
#include "resident/domains/director/world_codec/seed_original.inc"
#include "resident/domains/tasks/world_codec/seed_dynamic.inc"
      if (!result.social->dynamic)
        return {false,
                {},
                common_failure("MissingDynamicSocial", "social/dynamic").error};
      auto social_layout =
          validate_native_social_state_selection(*result.social, catalog);
      if (!social_layout.ok)
        return {false, {}, std::move(social_layout.error)};
    } else
      for (const auto *name : {"local_economy", "organization", "affairs",
                               "director", "dynamic_tasks", "dynamic_social"})
        if (source.find(name))
          return {
              false, {}, common_failure("UnexpectedDynamicState", name).error};
#include "resident/domains/organization/world_codec/seed_original_payloads.inc"
#include "resident/domains/military/world_codec/seed_population.inc"
#include "resident/domains/politics/world_codec/seed_appointments_takeover.inc"
#include "resident/domains/tasks/world_codec/seed_work_contracts.inc"
#include "resident/domains/growth/world_codec/seed_market_history.inc"
#include "resident/domains/politics/world_codec/seed_licensing_continuations.inc"
#include "resident/domains/commerce/world_codec/seed_rent_shops.inc"
#include "resident/domains/tasks/world_codec/seed_shop_contacts.inc"
#include "resident/domains/commerce/world_codec/seed_liquidations.inc"
#include "resident/domains/family/world_codec/seed_legacy_paths.inc"
#include "resident/domains/tasks/world_codec/seed_personal_contracts.inc"
#include "resident/domains/politics/world_codec/seed_successions.inc"
#include "resident/domains/commerce/world_codec/seed_command_results.inc"
#include "resident/domains/politics/world_codec/seed_office_handovers.inc"
#include "resident/domains/player_plan/world_codec/seed_original.inc"
#include "resident/domains/politics/world_codec/seed_governance_fiscal.inc"
#include "resident/domains/local_economy/world_codec/seed_household_fiscal.inc"
#include "resident/domains/tasks/world_codec/seed_envoy_governance.inc"
#include "resident/domains/politics/world_codec/seed_governance_sidecar.inc"
#include "resident/domains/local_economy/world_codec/seed_public_relief.inc"
#include "resident/domains/politics/world_codec/seed_diplomacy_reports.inc"
#include "resident/domains/commerce/world_codec/seed_validate_shop_payloads.inc"
#include "resident/domains/family/world_codec/seed_validate_apprenticeship.inc"
#include "resident/domains/politics/world_codec/seed_validate_legal_payloads.inc"
#include "resident/domains/growth/world_codec/seed_validate_recovery.inc"
#include "resident/domains/interaction/world_codec/seed_validate_original_dialogues.inc"
#include "resident/domains/family/world_codec/seed_voluntary_paths.inc"
#include "resident/domains/interaction/world_codec/seed_validate_known_dialogues.inc"
#include "resident/domains/tasks/world_codec/seed_validate_shop_contacts.inc"
#include "resident/domains/family/world_codec/seed_validate_collateral.inc"
#include "resident/domains/tasks/world_codec/seed_validate_personal_payloads.inc"
#include "resident/domains/interaction/world_codec/seed_validate_visit_appointments.inc"
    for (const auto &row : result.common.actions.rows())
      if (row.second.kind == "wait" ||
          (family_birth_enabled(catalog) &&
           row.second.kind == "family_child_care") ||
          (family_care_relief_enabled(catalog) &&
           row.second.kind == "family_child_care_relief"))
        required_empty(boundary::field(boundary::field(source, "actions", ""),
                                       row.first, "actions"),
                       "params");
    {
      CommonStore original(result.common, catalog.common);
      auto inspection = validate_native_public_supply_inspection_layout(
          catalog, original, result.public_supply_inspection.get());
      if (!inspection.ok)
        return inspection;
      for (const auto &row : result.common.facts.rows())
        if (row.second.kind == public_supply_inspection_source_kind)
          fields(field(field(source, "facts", "state"), row.first, "facts"),
                 {"kind", "domain"}, {}, row.first);
      auto checked = validate_native_condition_application_headers(
          original, catalog, result.actor_foundation.get());
      if (!checked.ok)
        return checked;
    }
    auto field_layout = validate_field_battle_layout(catalog.identity,
                                                     catalog.field_battle.get(),
                                                     result.field_battle.get());
    if (!field_layout.ok)
      return field_layout;
    {
      CommonStore original(result.common, catalog.common);
      auto checked = validate_field_battle_fact_headers(original, result.field_battle.get());
      if (!checked.ok)
        return checked;
      // 此时只有 InitialWorldSeed，不能构造缺 abilities 的业务 reader。
      // Complete 的完整同 World 门在 install_seed 候选 validate 后才准发布；
      // 非 Complete 的原旧语义仍在这里执行。
      if (result.field_battle &&
          !(catalog.exact_time && exact_complete_selected(*catalog.exact_time))) {
        checked = validate_field_battle_world(original, *result.military,
            *catalog.travel, *catalog.field_battle, *result.field_battle);
        if (!checked.ok)
          return checked;
      }
      for (const auto &row : result.common.facts.rows())
        if (owns_field_battle_fact_kind(row.second.kind))
          fields(field(field(source, "facts", "state"), row.first, "facts"),
                 {"kind", "domain"}, {}, row.first);
    }
    return common_success();
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace
