// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
namespace {
// 同一真实域派发表同时服务旧Bytes与冻结snapshot的两遍stream，不能另维护rest专用白名单。
template <class Consume>
Status visit_world_checkpoint_sections(WorldBundle &w, Consume &&consume) {
  auto simultaneous = validate_personal_simultaneous_outcome_modules(
      w.catalog->identity.module_versions,
      w.catalog->personal &&
          !w.catalog->personal->simultaneous_outcome_objectives.empty());
  if (!simultaneous.ok)
    return {false, {}, simultaneous.error};
  auto political_body =
      read_political_locked_body_version(w.catalog->identity, "politics");
  auto fiscal_body = read_political_locked_body_version(w.catalog->identity,
                                                        "political_fiscal");
  auto takeover_body = read_political_locked_body_version(w.catalog->identity,
                                                          "political_takeover");
  if (!political_body.ok)
    return {false, {}, std::move(political_body.error)};
  if (!fiscal_body.ok)
    return {false, {}, std::move(fiscal_body.error)};
  if (!takeover_body.ok)
    return {false, {}, std::move(takeover_body.error)};
  ResidentResultCodec receipts;
  const auto &selected_domains = domains_for(*w.catalog);
  // 写入和读取只用同一版本表，新增typed载荷不能生成本版本自身拒绝的检查点。
#include "resident/domains/politics/world_codec/checkpoint_write_version_guards.inc"
  auto section = [&](const StableId &name, auto visit) -> Status {
    auto schema =
        std::find_if(selected_domains.begin(), selected_domains.end(),
                     [&](const auto &domain) { return domain.name == name; });
    if (schema == selected_domains.end())
      return common_failure("UnknownCheckpointDomain", name,
                            ErrorKind::internal);
    return consume(
        name, schema->codec_version, [&](CodecVisitor &writer) -> Status {
          auto resources =
              configure_domain_resource_references(writer, w.catalog->identity);
          if (!resources.ok)
            return resources;
          writer.political_head_handover_codec_version = handover_version.value;
          writer.political_term_handover_codec_version = term_version.value;
          writer.political_office_reception_codec_version =
              reception_version.value;
          writer.political_office_resignation_codec_version =
              resignation_version.value;
          writer.political_subordinate_appointment_codec_version =
              subordinate_version.value;
          writer.political_governance_agency_codec_version =
              agency_version.value;
          writer.political_successor_round_codec_version = round_version.value;
          writer.political_succession_codec_version = succession_version.value;
          if (name == "military") {
            auto base = military_checkpoint_base_version(
                schema->codec_version,
                w.catalog->selected_military_occupation_schema);
            if (!base.ok)
              return {false, {}, std::move(base.error)};
            if (base.value == 16 || base.value == 17)
              writer.military_codec_version = base.value;
          }
          if (w.private_shop_daily_rent_enabled())
            writer.schema_features |=
                private_shop_daily_rent_checkpoint_feature;
          if (w.catalog->political_governance_invitation_replies)
            writer.schema_features |= governance_invitation_reply_feature;
          if (w.catalog->political_governance_continuations)
            writer.schema_features |= political_governance_continuation_feature;
          if (w.private_shops_enabled() && w.political_shop_licensing_enabled())
            writer.schema_features |= private_shop_checkpoint_feature;
          if (w.political_shop_registry_successions_enabled())
            writer.schema_features |=
                political_shop_registry_succession_checkpoint_feature;
          if (w.private_shop_liquidations_enabled())
            writer.schema_features |=
                private_shop_liquidation_checkpoint_feature;
          if (w.economic_supply_policies_enabled())
            writer.schema_features |= economic_supply_policy_checkpoint_feature;
          if (officers_enabled(*w.catalog))
            writer.schema_features |= military_officer_checkpoint_feature;
          if (field_commands_enabled(*w.catalog))
            writer.schema_features |= military_field_command_checkpoint_feature;
          if (military_recovery_enabled(*w.catalog))
            writer.schema_features |= military_recovery_checkpoint_feature;
          if (military_supply_attrition_enabled(*w.catalog))
            writer.schema_features |=
                military_supply_attrition_checkpoint_feature;
          if (military_rout_enabled(*w.catalog))
            writer.schema_features |= military_rout_checkpoint_feature;
          if (military_command_shock_enabled(*w.catalog))
            writer.schema_features |= military_command_shock_checkpoint_feature;
          if (military_garrison_rest_enabled(*w.catalog))
            writer.schema_features |= military_garrison_rest_checkpoint_feature;
          if (military_combat_conditions_enabled(*w.catalog))
            writer.schema_features |=
                military_combat_conditions_checkpoint_feature;
          if (military_commanded_damage_enabled(*w.catalog))
            writer.schema_features |=
                military_commanded_damage_checkpoint_feature;
          if (military_command_resources_enabled(*w.catalog))
            writer.schema_features |=
                military_command_resources_checkpoint_feature;
          if (w.treaty_passage_enabled())
            writer.schema_features |= treaty_passage_checkpoint_feature;
          if (w.occupation_fiscal_enabled())
            writer.schema_features |= occupation_fiscal_checkpoint_feature;
          if (w.diplomacy_enabled() && w.diplomacy.version == 2)
            writer.schema_features |= diplomatic_report_checkpoint_feature;
          if (w.naval_enabled()) {
            writer.schema_features |= naval_command_codec::checkpoint_feature;
            writer.naval_codec_version =
                naval_codec::version(*w.catalog->naval);
          }
          if (requests_enabled(*w.catalog))
            writer.schema_features |=
                military_request_codec::checkpoint_feature;
          if (name == "growth" && w.catalog->growth->teaching_requests.enabled)
            writer.schema_features |=
                common_checkpoint::teaching_request_sources;
          if (name == "growth" &&
              w.catalog->growth->study_teaching_requests.enabled)
            writer.schema_features |=
                common_checkpoint::study_teaching_request_sources;
          if (name == "growth" && w.catalog->growth->innate_learning)
            writer.schema_features |= innate_study_checkpoint_feature;
          if (w.production_management_enabled())
            writer.schema_features |=
                common_checkpoint::production_configuration_receipts;
          if (household_provision_management_enabled(w))
            writer.schema_features |=
                common_checkpoint::household_provision_configuration_receipts;
          if (w.politics_enabled())
            writer.schema_features |= common_checkpoint::politics_receipts;
          if (w.political_fiscal_enabled())
            writer.schema_features |=
                common_checkpoint::political_fiscal_receipts;
          if (w.dynamic_enabled())
            writer.schema_features |=
                common_checkpoint::typed_resource_references;
          if (w.named_membership_enabled())
            writer.schema_features |=
                common_checkpoint::named_membership_receipts;
          if (name == "common") {
            auto selected = configure_common_physical_codec(
                writer, schema->codec_version, w.catalog->identity);
            if (!selected.ok)
              return selected;
          }
          return visit(writer);
        });
  };
  Status r = section("common", [&](auto &c) {
    return visit_common_checkpoint(c, w.common.state(), &receipts);
  });
  if (!r.ok)
    return {false, {}, r.error};
  auto delivery_version =
      profile_delivery_selected_version(w.catalog->identity);
  if (!delivery_version.ok)
    return {false, {}, std::move(delivery_version.error)};
  if (delivery_version.value == 1) {
    r = section("profile_deliveries", [&](auto &c) {
      return visit_profile_delivery_outbox(c, w.profile_deliveries);
    });
    if (!r.ok)
      return {false, {}, r.error};
  } else if (!w.profile_deliveries.entries.empty())
    return {false, {}, common_failure("UnexpectedProfileDeliveries").error};
#include "resident/domains/military/world_codec/checkpoint_write_original.inc"
#include "resident/domains/travel/world_codec/checkpoint_write_transport.inc"
  r = section("interaction", [&](auto &c) {
    return visit_native_interaction_checkpoint(c, w.interaction, *w.catalog);
  });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("social", [&](auto &c) {
    return visit_native_social_checkpoint(c, w.social, *w.catalog);
  });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("estate",
              [&](auto &c) { return visit_estate_checkpoint(c, w.estate); });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("tasks",
              [&](auto &c) { return visit_task_checkpoint(c, w.tasks); });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("family", [&](auto &c) {
    return visit_family_checkpoint_versioned(
        c, w.family, family_version_for(*w.catalog),
        w.catalog->common->extended_skills);
  });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("personal", [&](auto &c) {
    return visit_personal_checkpoint(c, w.personal);
  });
  if (!r.ok)
    return {false, {}, r.error};
  {
    auto layout = validate_native_personal_battle_lifecycle_layout(w);
    if (!layout.ok)
      return layout;
    auto selected =
        selected_native_personal_battle_lifecycle(w.catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (selected.value) {
      r = section("personal_battle_lifecycle", [&](auto &c) {
        return visit_personal_battle_lifecycle(c, w.personal, selected.value);
      });
      if (!r.ok)
        return r;
    }
  }
#include "resident/domains/growth/world_codec/checkpoint_write_original.inc"
  if (w.catalog->growth->complete_capability_application_chain) {
    r = section("trade-grant", [&](auto &c) {
      return visit_growth_trade_grants_v1(c, w.growth.trade_grants);
    });
    if (!r.ok)
      return r;
  } else if (!w.growth.trade_grants.empty())
    return common_failure("UnexpectedGrowthTradeGrants", "trade-grant");
  // 原 Complete 终态表使用独立 /1 段，不改变旧 Growth 字节。
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    r = section("growth_terminal_publications", [&](auto &c) {
      return visit_growth_terminal_publications_v1(
          c, w.growth.terminal_publications);
    });
    if (!r.ok)
      return r;
  } else if (!w.growth.terminal_publications.empty())
    return common_failure("UnexpectedGrowthTerminalPublications");
  if (w.dynamic_enabled()) {
#include "resident/domains/affairs/world_codec/checkpoint_write_original.inc"
#include "resident/domains/director/world_codec/checkpoint_write_original.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_write_obligations.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_write_original.inc"
#include "resident/domains/organization/world_codec/checkpoint_write_runtime.inc"
#include "resident/domains/social/world_codec/checkpoint_write_dynamic.inc"
#include "resident/domains/tasks/world_codec/checkpoint_write_dynamic.inc"
  }
  if (w.population_military_enabled()) {
    r = section("population_military", [&](auto &c) {
      return visit_population_military_checkpoint(
          c, w.population_military, population_version(*w.catalog),
          w.siege_integration_enabled());
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.siege_integration_enabled()) {
    r = section("siege_integration", [&](auto &c) {
      return visit_siege_integration_checkpoint(c,
                                                w.military.siege_integration);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("military_integration", [&](auto &c) {
      return visit_military_integration_facts(c, w.military.integration_facts);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.player_plan_enabled()) {
    r = section("player_plan", [&](auto &c) {
      return visit_player_plan_checkpoint(c, w.player_plan);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.politics_enabled()) {
    r = section("politics", [&](auto &c) {
      return political_body.value == 2
                 ? visit_politics2(c, w.politics)
                 : visit_politics_checkpoint(c, w.politics);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.political_fiscal_enabled()) {
    r = section("political_fiscal", [&](auto &c) {
      return fiscal_body.value == 2
                 ? visit_political_fiscal_checkpoint2(c, w.political_fiscal)
                 : visit_political_fiscal_checkpoint(c, w.political_fiscal);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.fiscal_settlement_enabled()) {
    r = section("fiscal_settlement", [&](auto &c) {
      return visit_fiscal_settlement_checkpoint(c, w.fiscal_settlement);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.politics_governance_enabled()) {
    r = section("politics_governance", [&](auto &c) {
      return visit_politics_governance_checkpoint(c, w.politics_governance);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("governance_tasks", [&](auto &c) {
      return visit_governance_task_checkpoint(c, w.governance_tasks);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("public_relief", [&](auto &c) {
      return visit_public_relief_checkpoint(c, w.public_relief);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_offer_responses) {
    if (!w.tasks.offer_responses)
      return {
          false, {}, common_failure("MissingTaskOfferResponseDomain").error};
    r = section("task_offer_responses", [&](auto &c) {
      return visit_task_offer_response_checkpoint(c, *w.tasks.offer_responses);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_personal_advances) {
    if (!w.tasks.personal_advances)
      return {false, {}, common_failure("MissingTaskAdvanceDomain").error};
    r = section("task_personal_advances", [&](auto &c) {
      return visit_task_personal_advance_checkpoint(c,
                                                    *w.tasks.personal_advances);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_message_handoffs_enabled) {
    if (!w.tasks.message_handoffs)
      return {
          false, {}, common_failure("MissingTaskMessageHandoffDomain").error};
    r = section("task_message_handoffs", [&](auto &c) {
      return visit_task_message_handoffs_checkpoint(c,
                                                    *w.tasks.message_handoffs);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.diplomacy_enabled()) {
    r = section("diplomacy", [&](auto &c) {
      return visit_diplomacy_checkpoint(c, w.diplomacy);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("envoy_tasks", [&](auto &c) {
      return visit_envoy_task_checkpoint(c, w.envoy_tasks);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.naval_enabled()) {
    r = section("military_naval", [&](auto &c) {
      return naval_codec::visit(c, w.naval, *w.catalog->naval);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (household_provision_management_enabled(w)) {
    r = section("household_provision_management", [&](auto &c) {
      return visit_household_provision_management_checkpoint(
          c, w.household_provision_management);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.production_disruptions_enabled()) {
    r = section("production_disruptions", [&](auto &c) {
      return visit_production_disruption_checkpoint(c,
                                                    w.production_disruptions);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.political_takeover_enabled()) {
    r = section("political_takeover", [&](auto &c) {
      return takeover_body.value == 2
                 ? visit_political_takeover_checkpoint2(c, w.political_takeover)
                 : visit_political_takeover_checkpoint(c, w.political_takeover);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("economic_takeover_execution", [&](auto &c) {
      return visit_economic_takeover_execution_checkpoint(
          c, w.economic_takeover_execution);
    });
    if (!r.ok)
      return {false, {}, r.error};
    if (!w.tasks.takeover_execution)
      return {
          false, {}, common_failure("MissingTaskTakeoverExecutionState").error};
    r = section("task_takeover_execution", [&](auto &c) {
      return visit_task_public_account_execution_checkpoint(
          c, *w.tasks.takeover_execution);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.family_life_enabled()) {
    r = section("family_life", [&](auto &c) {
      return w.catalog->family_life->selected_version == 4
                 ? visit_family_life4(c, w.family_life)
                 : visit_family_deaths_checkpoint(c, w.family_life);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.family_legacy_enabled()) {
    r = section("family_legacy", [&](auto &c) {
      return w.catalog->family_legacy->version == 3
                 ? visit_family_legacy_checkpoint_v3(c, w.family_legacy)
             : w.catalog->family_legacy->version == 2
                 ? visit_family_legacy_checkpoint_v2(c, w.family_legacy)
                 : visit_family_legacy_checkpoint(c, w.family_legacy);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.occupation_fiscal_enabled()) {
    r = section("political_occupation_fiscal", [&](auto &c) {
      return visit_occupation_fiscal_checkpoint(c, w.occupation_fiscal);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("occupation_fiscal_settlement", [&](auto &c) {
      return visit_occupation_fiscal_settlement_checkpoint(
          c, w.occupation_fiscal_settlement);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.family_bequest_enabled()) {
    r = section("family_bequest", [&](auto &c) {
      return visit_family_bequests_checkpoint(c, w.family_bequest);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_message_notices) {
    if (!w.tasks.message_notices)
      return {
          false, {}, common_failure("MissingTaskMessageNoticeDomain").error};
    r = section("task_message_notices", [&](auto &c) {
      return visit_task_message_notices_checkpoint(c, *w.tasks.message_notices);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.treaty_passage_enabled()) {
    r = section("treaty_passage", [&](auto &c) {
      return visit_treaty_passage_checkpoint(c, w.treaty_passage);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.family_inheritance_enabled()) {
    r = section("family_inheritance", [&](auto &c) {
      return visit_family_inheritance_checkpoint(c, w.family_inheritance);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.fiscal_taxpayer_continuity_enabled()) {
    r = section("fiscal_taxpayer_continuity", [&](auto &c) {
      return visit_fiscal_taxpayer_continuity_checkpoint(
          c, w.fiscal_taxpayer_continuity);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.political_military_appointments_enabled()) {
    r = section(military_officer_political_module, [&](auto &c) {
      return visit_political_military_appointments_checkpoint(
          c, w.political_military_appointments,
          w.political_field_appointments_enabled());
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.family_marriage_enabled()) {
    r = section("family_marriage", [&](auto &c) {
      return visit_family_marriage_checkpoint(c, w.family_marriage);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_static_assets_enabled) {
    if (!w.tasks.static_assets)
      return {false, {}, common_failure("MissingTaskStaticAssetDomain").error};
    r = section("task_static_assets", [&](auto &c) {
      return visit_task_static_assets_checkpoint(c, *w.tasks.static_assets);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.estate_employment_enabled()) {
    r = section("estate_employment", [&](auto &c) {
      return visit_estate_employment_checkpoint(c, w.estate_employment);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.political_field_appointments_enabled()) {
    r = section(military_field_command_political_module, [&](auto &c) {
      return visit_political_field_appointments_checkpoint(
          c, w.political_field_appointments);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.economic_supply_policies_enabled()) {
    r = section("economic_supply_policies", [&](auto &c) {
      return visit_economic_supply_policy_checkpoint(
          c, w.economic_supply_policies);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("political_supply_adoptions", [&](auto &c) {
      return visit_political_supply_adoptions_checkpoint(
          c, w.political_supply_adoptions);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.family_birth_enabled()) {
    r = section("family_birth", [&](auto &c) {
      return visit_native_family_birth(
          c, w.family_birth, w.catalog->family_birth->selected_version);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->interaction->visit_messages) {
    if (!w.interaction.visit_messages)
      return {false, {}, common_failure("MissingVisitMessageDomain").error};
    r = section("visit_messages", [&](auto &c) {
      return visit_visit_messages_checkpoint(c, *w.interaction.visit_messages);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->director && w.catalog->director->visit_reception) {
    if (!w.interaction.visit_reception_claims)
      return {false, {}, common_failure("MissingVisitReceptionClaims").error};
    r = section("visit_reception_claims", [&](auto &c) {
      return visit_visit_reception_checkpoint(
          c, *w.interaction.visit_reception_claims);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.family_care_relief_enabled()) {
    r = section("family_care_relief", [&](auto &c) {
      return visit_child_care_relief_checkpoint(c, w.child_care_relief);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.growth_market_history_enabled()) {
    r = section("growth_market_history", [&](auto &c) {
      return w.catalog->growth_market_history->selected_version == 2
                 ? visit_growth_market_history_v2_checkpoint(
                       c, w.growth_market_history,
                       w.catalog->growth_market_history->resource_codec)
                 : visit_growth_market_history_checkpoint(
                       c, w.growth_market_history);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.political_shop_licensing_enabled()) {
    auto selected =
        selected_native_political_shop_licensing_version(w.catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (!selected.value || selected.value != w.political_shop_licensing.version)
      return {
          false,
          {},
          common_failure("PoliticalShopLicensingCheckpointSelectionMismatch")
              .error};
    r = section("political_shop_licensing", [&](auto &c) {
      return selected.value == 2 ? visit_political_shop_licensing_checkpoint2(
                                       c, w.political_shop_licensing)
                                 : visit_political_shop_licensing_checkpoint(
                                       c, w.political_shop_licensing);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.private_shops_enabled()) {
    r = section("private_shops", [&](auto &c) {
      return visit_private_shop_checkpoint(c, w.private_shops);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.task_shop_orders_enabled()) {
    if (!w.tasks.shop_orders)
      return {false, {}, common_failure("MissingTaskShopOrderDomain").error};
    r = section("task_shop_orders", [&](auto &c) {
      return visit_task_shop_orders_checkpoint(c, *w.tasks.shop_orders);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_extensions) {
    if (!w.tasks.deadline_extensions)
      return {false, {}, common_failure("MissingTaskExtensionDomain").error};
    r = section("task_extensions", [&](auto &c) {
      return visit_task_extensions_checkpoint(c, *w.tasks.deadline_extensions);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.political_shop_registry_successions_enabled()) {
    r = section("political_shop_registry_successions", [&](auto &c) {
      return visit_political_shop_registry_successions_checkpoint(
          c, w.political_shop_registry_successions);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->personal_appointments) {
    if (!w.interaction.personal_appointments)
      return {
          false, {}, common_failure("MissingPersonalAppointmentDomain").error};
    r = section("personal_appointments", [&](auto &c) {
      return visit_personal_appointments_checkpoint(
          c, *w.interaction.personal_appointments);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.personal_recovery_enabled()) {
    r = section("personal_injury_recovery", [&](auto &c) {
      return visit_growth_recovery_checkpoint(c, w.personal_recovery);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_shop_licensing_receipts) {
    if (!w.political_shop_licensing_receipts)
      return {false,
              {},
              common_failure("PoliticalShopLicensingReceiptsProfileMismatch")
                  .error};
    r = section("political_shop_licensing_receipts", [&](auto &c) {
      return visit_political_shop_licensing_receipts_checkpoint(
          c, *w.political_shop_licensing_receipts);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.private_shop_liquidations_enabled()) {
    if (!w.tasks.shop_orders || !w.tasks.shop_orders->estate_cancellations)
      return {false,
              {},
              common_failure("TaskShopOrderEstateProfileMismatch").error};
    r = section("private_shop_liquidations", [&](auto &c) {
      return visit_private_shop_liquidations_checkpoint(
          c, w.private_shop_liquidations);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("task_shop_order_estate_cancellations", [&](auto &c) {
      return visit_task_shop_order_estate_cancellations_checkpoint(
          c, *w.tasks.shop_orders->estate_cancellations);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->family_apprenticeship) {
    r = section("family_apprenticeship", [&](auto &c) {
      return visit_family_apprenticeship_checkpoint(c, w.family_apprenticeship);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_governance_continuations) {
    if (!w.political_governance_continuations)
      return {false,
              {},
              common_failure("GovernanceContinuationProfileRequired").error};
    r = section("political_governance_continuations", [&](auto &c) {
      return visit_political_governance_continuations_checkpoint(
          c, *w.political_governance_continuations);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->contact_inquiries) {
    if (!w.interaction.contact_inquiries)
      return {false, {}, common_failure("MissingContactInquiryDomain").error};
    r = section("contact_inquiries", [&](auto &c) {
      return visit_contact_inquiries_checkpoint(
          c, *w.interaction.contact_inquiries);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->visit_message_replies) {
    if (!w.interaction.visit_message_replies)
      return {false, {}, common_failure("MissingVisitReplyDomain").error};
    r = section("visit_message_replies", [&](auto &c) {
      return visit_visit_replies_checkpoint(
          c, *w.interaction.visit_message_replies);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->family_disciple_legacy) {
    r = section("family_disciple_legacy", [&](auto &c) {
      return visit_disciple_control_checkpoint(c, w.family_disciple_legacy);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_governance_invitation_replies) {
    if (!w.governance_invitation_replies)
      return {false,
              {},
              common_failure("GovernanceInvitationProfileRequired").error};
    r = section(governance_invitation_reply_module, [&](auto &c) {
      return visit_governance_invitation_replies_checkpoint(
          c, *w.governance_invitation_replies);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.private_shop_daily_rent_enabled()) {
    r = section(private_shop_daily_rent_module, [&](auto &c) {
      return visit_private_shop_daily_rent_checkpoint(
          c, w.private_shop_daily_rent);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->born_personal_dialogues) {
    if (!w.interaction.born_personal_dialogues)
      return {
          false, {}, common_failure("MissingBornPersonalDialogueDomain").error};
    r = section("born_personal_dialogues", [&](auto &c) {
      return visit_born_personal_dialogues_checkpoint(
          c, *w.interaction.born_personal_dialogues);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->family_biological_legacy) {
    r = section("family_biological_legacy", [&](auto &c) {
      return visit_biological_control_checkpoint(c, w.family_biological_legacy);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_successions) {
    if (!w.political_successions)
      return {false,
              {},
              common_failure("PoliticalSuccessionProfileRequired").error};
    r = section(political_succession_domain, [&](auto &c) {
      return visit_political_succession_checkpoint(c, *w.political_successions);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->visit_appointment_invitations) {
    if (!w.interaction.visit_appointment_invitations ||
        !w.interaction.visit_appointment_meetings)
      return {false, {}, common_failure("MissingVisitAppointmentDomain").error};
    r = section("visit_appointment_invitations", [&](auto &c) {
      return visit_visit_appointments_checkpoint(
          c, *w.interaction.visit_appointment_invitations);
    });
    if (!r.ok)
      return {false, {}, r.error};
    r = section("visit_appointment_meetings", [&](auto &c) {
      return visit_visit_appointment_meetings_checkpoint(
          c, *w.interaction.visit_appointment_meetings);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_head_handovers) {
    if (!w.political_head_handovers)
      return {false,
              {},
              common_failure("PoliticalHeadHandoverProfileRequired").error};
    r = section(political_head_handover_domain, [&](auto &c) {
      return visit_political_head_handover_checkpoint(
          c, *w.political_head_handovers);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.military_tactic_learning_enabled()) {
    r = section(military_tactic_learning_module, [&](auto &c) {
      return military_tactic_learning_codec::visit(
          c, w.military_tactic_learning, 1);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.growth_tactic_lessons_enabled()) {
    r = section("growth_tactic_lessons", [&](auto &c) {
      return visit_growth_tactic_lessons_checkpoint(c, w.growth_tactic_lessons);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_successor_rounds) {
    if (!w.political_successor_rounds)
      return {false,
              {},
              common_failure("PoliticalSuccessorRoundProfileRequired").error};
    r = section(political_successor_rounds_domain, [&](auto &c) {
      return visit_political_successor_round_checkpoint(
          c, *w.political_successor_rounds);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.private_shop_command_results_enabled()) {
    r = section(private_shop_command_results_module, [&](auto &c) {
      return visit_private_shop_command_results_checkpoint(
          c, w.private_shop_command_results);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_term_handovers) {
    if (!w.political_term_handovers)
      return {false,
              {},
              common_failure("PoliticalTermHandoverProfileRequired").error};
    r = section(political_term_handover_domain, [&](auto &c) {
      return visit_political_term_handover_checkpoint(
          c, *w.political_term_handovers);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->visit_message_receipts_enabled) {
    if (!w.interaction.visit_message_receipts)
      return {
          false, {}, common_failure("MissingVisitMessageReceiptDomain").error};
    r = section(visit_message_receipt_module, [&](auto &c) {
      return visit_visit_message_receipts_checkpoint(
          c, *w.interaction.visit_message_receipts);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->known_personal_dialogues) {
    if (!w.interaction.known_personal_dialogues)
      return {false,
              {},
              common_failure("MissingKnownPersonalDialogueDomain").error};
    r = section("known_personal_dialogues", [&](auto &c) {
      return visit_known_personal_dialogues_checkpoint(
          c, *w.interaction.known_personal_dialogues);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_office_receptions) {
    if (!w.political_office_receptions)
      return {false,
              {},
              common_failure("PoliticalOfficeReceptionProfileRequired").error};
    r = section(political_office_reception_domain, [&](auto &c) {
      return visit_political_office_reception_checkpoint(
          c, *w.political_office_receptions);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_shop_order_contacts_enabled) {
    if (!w.tasks.shop_orders || !w.tasks.shop_orders->contacts)
      return {
          false, {}, common_failure("MissingTaskShopOrderContactDomain").error};
    r = section(task_shop_order_contact_module, [&](auto &c) {
      return visit_task_shop_order_contacts_checkpoint(
          c, *w.tasks.shop_orders->contacts);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->family_collateral_legacy) {
    r = section("family_collateral_legacy", [&](auto &c) {
      return visit_collateral_control_checkpoint(c, w.family_collateral_legacy);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_governance_agencies) {
    if (!w.political_governance_agencies)
      return {false,
              {},
              common_failure("PoliticalGovernanceAgencyProfileRequired").error};
    r = section(political_governance_agency_domain, [&](auto &c) {
      return visit_political_governance_agency_checkpoint(
          c, *w.political_governance_agencies);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_message_returns) {
    if (!w.tasks.message_returns)
      return {
          false, {}, common_failure("MissingTaskMessageReturnDomain").error};
    r = section(task_message_return_module, [&](auto &c) {
      return visit_task_message_returns_checkpoint(c, *w.tasks.message_returns);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->family_voluntary_paths) {
    r = section("family_voluntary_paths", [&](auto &c) {
      return visit_voluntary_path_checkpoint(c, w.family_voluntary_paths);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_personal_messages) {
    if (!w.tasks.personal_messages)
      return {
          false, {}, common_failure("MissingTaskPersonalMessageDomain").error};
    r = section(task_personal_message_module, [&](auto &c) {
      return visit_task_personal_messages_checkpoint(
          c, *w.tasks.personal_messages);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_office_resignations) {
    if (!w.political_office_resignations)
      return {
          false,
          {},
          common_failure("PoliticalOfficeResignationProfileRequired").error};
    r = section(political_office_resignation_module, [&](auto &c) {
      return visit_political_office_resignation_checkpoint(
          c, *w.political_office_resignations);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_personal_reports) {
    if (!w.tasks.personal_reports)
      return {
          false, {}, common_failure("MissingTaskPersonalReportDomain").error};
    r = section(task_personal_report_module, [&](auto &c) {
      return visit_task_personal_reports_checkpoint(c,
                                                    *w.tasks.personal_reports);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_personal_cancellations) {
    if (!w.tasks.personal_cancellations)
      return {false,
              {},
              common_failure("MissingTaskPersonalCancellationDomain").error};
    r = section(task_personal_cancellation_module, [&](auto &c) {
      return visit_task_personal_cancellations_checkpoint(
          c, *w.tasks.personal_cancellations);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_subordinate_appointments) {
    if (!w.political_subordinate_appointments)
      return {false,
              {},
              common_failure("PoliticalSubordinateAppointmentProfileRequired")
                  .error};
    r = section(political_subordinate_appointment_module, [&](auto &c) {
      return visit_political_subordinate_appointment_checkpoint(
          c, *w.political_subordinate_appointments);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->task_personal_acceptances) {
    if (!w.tasks.personal_acceptances)
      return {false,
              {},
              common_failure("MissingTaskPersonalAcceptanceDomain").error};
    r = section(task_personal_acceptance_module, [&](auto &c) {
      return visit_task_personal_acceptances_checkpoint(
          c, *w.tasks.personal_acceptances);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
  if (w.catalog->political_dismissal_sources) {
    if (!w.political_dismissal_sources)
      return {false,
              {},
              common_failure("PoliticalDismissalSourceProfileRequired").error};
    r = section(political_dismissal_sources_module, [&](auto &c) {
      return visit_political_dismissal_sources(c,
                                               *w.political_dismissal_sources);
    });
    if (!r.ok)
      return {false, {}, r.error};
  }
#include "resident/domains/actor_foundation/world_codec/checkpoint_write_exact_origins.inc"
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    if (!w.facility_claims)
      return common_failure("MissingFacilityClaims");
    r = section("facility_claims", [&](auto &c) {
      return visit_facility_claims2(c, *w.facility_claims);
    });
    if (!r.ok)
      return r;
  } else if (w.facility_claims)
    return common_failure("UnexpectedFacilityClaims");
  // 使用原成对visitor保存P02/3劳力尾，不建立另一套状态或字段顺序。
  auto population_version =
      selected_native_population_transport_version(w.catalog->identity);
  if (!population_version.ok)
    return {false, {}, std::move(population_version.error)};
  if (bool(population_version.value) != bool(w.population_transport))
    return common_failure("PopulationTransportSelectedStateMismatch");
  if (population_version.value) {
    r = section("civilian_transport_population", [&](auto &c) {
      return visit_population_transport(c, *w.population_transport,
                                        bool(w.catalog->household_purchases),
                                        population_version.value);
    });
    if (!r.ok)
      return r;
  }
  if (w.catalog->household_purchases) {
    if (!w.household_purchases || !w.household_retail_settlements)
      return common_failure("MissingHouseholdSelectedState");
    r = section("household_purchases", [&](auto &c) {
      return visit_household_purchases(c, *w.household_purchases,
                                       w.catalog->selected_household_version);
    });
    if (!r.ok)
      return r;
    r = section("household_retail_settlements", [&](auto &c) {
      return visit_household_retail_checkpoint(c,
                                               *w.household_retail_settlements);
    });
    if (!r.ok)
      return r;
  } else if (w.household_purchases || w.household_retail_settlements)
    return common_failure("UnexpectedHouseholdState");
  if (w.catalog->exact_time &&
      (exact_social_selected(*w.catalog->exact_time) ||
       exact_complete_selected(*w.catalog->exact_time))) {
    if (!w.social_period_sources)
      return common_failure("MissingSocialPeriodSources");
    r = section("social_period_sources", [&](auto &c) {
      return visit_social_period_sources_checkpoint(c,
                                                    *w.social_period_sources);
    });
    if (!r.ok)
      return r;
  }
  r = validate_native_face_to_face_world(w);
  if (!r.ok)
    return r;
  if (w.interaction.face_to_face_contacts) {
    r = section("face_to_face_contacts", [&](auto &c) {
      return visit_face_to_face_contact_sources_checkpoint(
          c, *w.interaction.face_to_face_contacts);
    });
    if (!r.ok)
      return r;
  }
  r = validate_native_actor_social_occupancy(w);
  if (!r.ok)
    return r;
  if (w.catalog->actor_social_occupancy_selected) {
    auto version =
        selected_native_actor_social_occupancy_version(w.catalog->identity);
    if (!version.ok)
      return {false, {}, std::move(version.error)};
    if (!w.interaction.social_occupancy ||
        w.interaction.social_occupancy->version != version.value)
      return {false,
              {},
              common_failure("ActorSocialOccupancyModuleMismatch").error};
    r = section("actor_social_occupancy", [&](auto &c) {
      return visit_actor_social_occupancy_checkpoint(
          c, *w.interaction.social_occupancy);
    });
    if (!r.ok)
      return r;
  }
  r = validate_native_life_age_layout(w);
  if (!r.ok)
    return r;
  if (w.catalog->life_age_checks) {
    r = section("life_age_checks", [&](auto &c) {
      return visit_life_age_checks(c, *w.life_age_checks);
    });
    if (!r.ok)
      return r;
  }
  r = validate_native_control_continuation_layout(w);
  if (!r.ok)
    return r;
  if (w.control_continuation) {
    r = section("control_continuation", [&](auto &c) {
      return w.catalog->control_continuation->selected_version == 3
                 ? visit_control_continuation_v3(c, *w.control_continuation)
                 : visit_control_continuation_v2(c, *w.control_continuation);
    });
    if (!r.ok)
      return r;
  }
  r = validate_native_organization_runtime_layout(w);
  if (!r.ok)
    return r;
  r = validate_native_economic_obligation_layout(w);
  if (!r.ok)
    return r;
  r = validate_native_family_birth_layout(w);
  if (!r.ok)
    return r;
  r = validate_native_born_population_layout(w);
  if (!r.ok)
    return r;
  r = validate_native_governance_program_layout(w);
  if (!r.ok)
    return r;
  if (w.governance_programs) {
    r = section("governance_programs", [&](auto &c) {
      return visit_native_governance_programs(
          c, *w.governance_programs, w.governance_programs->schema_version);
    });
    if (!r.ok)
      return r;
  }
#include "resident/domains/craft/world_codec/checkpoint_write_item_craft.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_write_facility_roots.inc"
#include "resident/domains/military/world_codec/checkpoint_write_exact_actions.inc"
#include "resident/domains/tasks/world_codec/checkpoint_write_exact_execution.inc"
  r = validate_native_field_passage_layout(w);
  if (!r.ok) return r;
  if (w.political_passage) {
    r = section("political_passage", [&](auto &c) {
      return visit_field_passage_checkpoint(c, *w.political_passage, *w.political_safe_exit);
    });
    if (!r.ok) return r;
  }
  r = validate_native_political_relations_world(w);
  if (!r.ok)
    return r;
  if (w.political_relations) {
    r = section("political_relations", [&](auto &c) {
      return visit_political_relations(c, *w.political_relations);
    });
    if (!r.ok)
      return r;
  }
  r = validate_native_public_supply_inspection(w);
  if (!r.ok)
    return r;
  if (w.public_supply_inspection) {
    r = section("public_supply_inspection", [&](auto &c) {
      return visit_public_supply_inspection_checkpoint(
          c, *w.public_supply_inspection);
    });
    if (!r.ok)
      return r;
  }
#include "resident/domains/commerce/world_codec/checkpoint_write_shop_operations.inc"
#include "resident/domains/commerce/world_codec/checkpoint_write_transport_funding.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_write_private_shop_custody.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_write_production_cycles.inc"
#include "resident/domains/tasks/world_codec/checkpoint_write_work.inc"
  return common_success();
}
} // namespace
