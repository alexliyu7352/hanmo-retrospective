// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
namespace {
// One component route feeds the reference JSON tree and the no-tree shape sink.
// Every payload traverses its original owner writer before its keys can be
// merged.
// 合成必须匹配原发射器的整根或字段合同；不补空键根，也不放宽原schema拒绝。
Status emit_world_export_components(const WorldBundle &w,
                                    WorldExportComposition &composition) {
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
  if (bool(political_body.value) != w.politics_enabled() ||
      bool(fiscal_body.value) != w.political_fiscal_enabled() ||
      bool(takeover_body.value) != w.political_takeover_enabled())
    return common_failure("PoliticalLockedBodySelectionMismatch");
  using P = WorldExportPart;
#define WORLD_FIELDS(part, domain, ...)                                        \
  do {                                                                         \
    auto emitted = composition.fields(                                         \
        part, domain, [&](auto &out) -> Status { return __VA_ARGS__; });       \
    if (!emitted.ok)                                                           \
      return emitted;                                                          \
  } while (false)
#define WORLD_WHOLE(part, domain, ...)                                         \
  do {                                                                         \
    auto emitted = composition.whole(                                          \
        part, domain, [&](auto &out) -> Status { return __VA_ARGS__; });       \
    if (!emitted.ok)                                                           \
      return emitted;                                                          \
  } while (false)
  auto permanent = validate_native_growth_permanent_layout(w);
  if (!permanent.ok)
    return permanent;
  auto battle_lifecycle_sources =
      validate_native_military_battle_lifecycle_sources(w);
  if (!battle_lifecycle_sources.ok)
    return battle_lifecycle_sources;
  auto battle_lifecycle =
      selected_native_military_battle_lifecycle(w.catalog->identity);
  if (!battle_lifecycle.ok)
    return {false, {}, std::move(battle_lifecycle.error)};
  auto personal_lifecycle_sources =
      validate_native_personal_battle_lifecycle_sources(w);
  if (!personal_lifecycle_sources.ok)
    return personal_lifecycle_sources;
  auto personal_lifecycle =
      selected_native_personal_battle_lifecycle(w.catalog->identity);
  if (!personal_lifecycle.ok)
    return {false, {}, std::move(personal_lifecycle.error)};
  auto deliveries = profile_delivery_selected_version(w.catalog->identity);
  if (!deliveries.ok)
    return {false, {}, std::move(deliveries.error)};
  if (!deliveries.value && !w.profile_deliveries.entries.empty())
    return common_failure("UnexpectedProfileDeliveries", "profile_deliveries");
  ResidentResultCodec receipts;
  const auto schemas = domains_for(*w.catalog);
  const auto common_schema =
      std::find_if(schemas.begin(), schemas.end(),
                   [](const auto &row) { return row.name == "common"; });
  if (common_schema == schemas.end())
    return common_failure("MissingCommonCheckpointDomain");
  for (const auto &family_section :
       std::initializer_list<std::pair<const char *, Integer>>{
           {"family_birth", w.family_birth_enabled()
                                ? w.catalog->family_birth->selected_version
                                : 0},
           {"family_inheritance",
            w.family_inheritance_enabled()
                ? w.catalog->family_inheritance->selected_version
                : 0},
           {"family_bequest", w.family_bequest_enabled()
                                  ? w.catalog->family_bequest->selected_version
                                  : 0}}) {
    if (!family_section.second)
      continue;
    auto physical =
        std::find_if(schemas.begin(), schemas.end(), [&](const auto &row) {
          return row.name == family_section.first;
        });
    if (physical == schemas.end() ||
        physical->codec_version != family_section.second)
      return common_failure("FamilySelectedPhysicalCodecRequired",
                            family_section.first,
                            ErrorKind::unsupported_capability);
  }
  auto base_selection =
      decode_common_codec_selection(common_schema->codec_version);
  if (!base_selection.ok)
    return {false, {}, std::move(base_selection.error)};
  auto selected = select_common_codec(base_selection.value.base_version,
                                      w.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const auto &features = selected.value.features;
  CommonResourceCodecContext resource_context{
      features.item_instances, features.lot_properties,
      features.domain_activity, features.carrier_location};
  WORLD_FIELDS(P::common, "common",
               emit_common_state_fields(out, w.common.state(), &receipts,
                                        resource_context));
  if (deliveries.value == 1)
    WORLD_FIELDS(
        P::section, "profile_deliveries",
        emit_profile_delivery_outbox_fields(out, w.profile_deliveries));
  WORLD_WHOLE(P::split, "travel",
              emit_travel_state_projection(out, "", w.travel));
  WORLD_FIELDS(
      P::split, "military",
      emit_military_state_fields(
          out, w.military, w.catalog->selected_military_occupation_schema));
  if (battle_lifecycle.value)
    WORLD_WHOLE(P::section, "military_battle_lifecycle",
                emit_military_battle_lifecycle(out, "", w.military,
                                               battle_lifecycle.value));
  WORLD_FIELDS(P::split, "estate", emit_estate_state_fields(out, w.estate));
  auto task_headers = validate_task_original_payload_headers(w.common, w.tasks);
  if (!task_headers.ok)
    return task_headers;
  WORLD_FIELDS(P::split, "tasks", emit_task_state_fields(out, w.tasks));
  WORLD_FIELDS(P::split, "family",
               emit_family_state_versioned_fields(
                   out, w.family, family_version_for(*w.catalog)));
  WORLD_FIELDS(P::split, "personal",
               emit_personal_state_fields(out, w.personal));
  if (personal_lifecycle.value)
    WORLD_WHOLE(P::section, "personal_battle_lifecycle",
                emit_personal_battle_lifecycle(out, "", w.personal,
                                               personal_lifecycle.value));
#include "resident/domains/craft/world_codec/export_item_craft_payloads.inc"
#include "resident/domains/craft/world_codec/export_validate_original_headers.inc"
#include "resident/domains/growth/world_codec/export_original.inc"
  if (w.catalog->growth->complete_capability_application_chain) {
    WORLD_WHOLE(
        P::section, "trade-grant",
        emit_growth_trade_grants_v1_projection(out, "", w.growth.trade_grants));
  } else if (!w.growth.trade_grants.empty())
    return common_failure("UnexpectedGrowthTradeGrants", "trade-grant");
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    WORLD_WHOLE(P::section, "growth_terminal_publications",
                emit_growth_terminal_publications_v1_projection(
                    out, "", w.growth.terminal_publications));
  } else if (!w.growth.terminal_publications.empty())
    return common_failure("UnexpectedGrowthTerminalPublications");
#include "resident/domains/local_economy/world_codec/export_facility_fact_payloads.inc"
  WORLD_FIELDS(P::fact_payloads, "growth_market_history",
               emit_shop_market_fact_payloads_fields(
                   out, w.common, *w.catalog,
                   w.growth_market_history_enabled() ? &w.growth_market_history
                                                     : nullptr,
                   w.private_shops_enabled() ? &w.private_shops : nullptr,
                   w.tasks.shop_orders ? &*w.tasks.shop_orders : nullptr,
                   w.private_shop_operation_sources
                       ? &*w.private_shop_operation_sources
                       : nullptr,
                   w.private_shop_operation_capabilities
                       ? &*w.private_shop_operation_capabilities
                       : nullptr));
  WORLD_FIELDS(P::fact_payloads, "private_shop_liquidations",
               emit_shop_estate_fact_payloads_fields(
                   out, w.common, *w.catalog,
                   w.private_shop_liquidations_enabled()
                       ? &w.private_shop_liquidations
                       : nullptr,
                   w.tasks.shop_orders ? &*w.tasks.shop_orders : nullptr));
  WORLD_FIELDS(P::fact_payloads, "growth_tactic_lessons",
               emit_growth_tactic_lesson_fact_payloads_fields(
                   out, w.common,
                   w.growth_tactic_lessons_enabled() ? &w.growth_tactic_lessons
                                                     : nullptr));
  WORLD_FIELDS(P::fact_payloads, "family_apprenticeship",
               emit_apprenticeship_fact_payloads_fields(
                   out, w.common, *w.catalog,
                   w.catalog->family_apprenticeship ? &w.family_apprenticeship
                                                    : nullptr));
  WORLD_FIELDS(P::action_payloads, "private_shops",
               emit_shop_action_payloads_fields(
                   out, w.common, *w.catalog,
                   w.private_shops_enabled() ? &w.private_shops : nullptr,
                   w.tasks.shop_orders ? &*w.tasks.shop_orders : nullptr));
#include "resident/domains/interaction/world_codec/export_dialogue_appointment_payloads.inc"
  WORLD_FIELDS(P::fact_payloads, "family_voluntary_paths",
               emit_voluntary_path_fact_payloads_fields(
                   out, w.common,
                   w.catalog->family_voluntary_paths ? &w.family_voluntary_paths
                                                     : nullptr));
  WORLD_FIELDS(P::fact_payloads, "task_shop_order_contacts",
               emit_task_shop_order_contact_fact_payloads_fields(
                   out, w.common,
                   w.tasks.shop_orders && w.tasks.shop_orders->contacts
                       ? &*w.tasks.shop_orders->contacts
                       : nullptr));
  WORLD_FIELDS(P::fact_payloads, "family_collateral_legacy",
               emit_collateral_recognition_fact_payloads_fields(
                   out, w.common,
                   w.family_collateral_legacy_enabled()
                       ? &w.family_collateral_legacy
                       : nullptr));
  WORLD_FIELDS(
      P::fact_payloads, "task_message_returns",
      emit_task_message_return_fact_payloads_fields(
          out, w.common,
          w.tasks.message_returns ? &*w.tasks.message_returns : nullptr));
  WORLD_FIELDS(
      P::action_payloads, "task_message_returns",
      emit_task_message_return_action_payloads_fields(
          out, w.common,
          w.tasks.message_returns ? &*w.tasks.message_returns : nullptr));
  WORLD_FIELDS(
      P::fact_payloads, task_personal_message_module,
      emit_task_personal_message_fact_payloads_fields(
          out, w.common,
          w.tasks.personal_messages ? &*w.tasks.personal_messages : nullptr));
  WORLD_FIELDS(
      P::fact_payloads, task_personal_report_module,
      emit_task_personal_report_fact_payloads_fields(
          out, w.common,
          w.tasks.personal_reports ? &*w.tasks.personal_reports : nullptr));
  WORLD_FIELDS(P::fact_payloads, task_personal_acceptance_module,
               emit_task_personal_acceptance_fact_payloads_fields(
                   out, w.common,
                   w.tasks.personal_acceptances ? &*w.tasks.personal_acceptances
                                                : nullptr));
  WORLD_FIELDS(P::fact_payloads, task_personal_cancellation_module,
               emit_task_personal_cancellation_fact_payloads_fields(
                   out, w.common,
                   w.tasks.personal_cancellations
                       ? &*w.tasks.personal_cancellations
                       : nullptr));
  if (w.dynamic_enabled())
    WORLD_FIELDS(P::fact_payloads, "dynamic_tasks",
                 emit_task_cargo_facts_fields(out, w.tasks));
  WORLD_FIELDS(P::fact_payloads, "task_execution_time",
               emit_native_task_execution_fact_payloads_fields(
                   out, w.common, *w.catalog, w.tasks));
  WORLD_FIELDS(
      P::root_fields, "interaction",
      emit_native_interaction_state_fields(out, w.interaction, *w.catalog));
  WORLD_FIELDS(P::root_fields, "social",
               emit_native_social_state_fields(out, w.social, *w.catalog));
  if (w.population_military_enabled())
    WORLD_FIELDS(P::population_actions, "population_military",
                 emit_population_military_state_fields(
                     out, w.population_military, population_version(*w.catalog),
                     w.siege_integration_enabled()));
  WORLD_FIELDS(P::action_payloads, "growth_tactic_lessons",
               emit_growth_tactic_lesson_action_payloads_fields(
                   out, w.common,
                   w.growth_tactic_lessons_enabled() ? &w.growth_tactic_lessons
                                                     : nullptr));
  if (w.naval_repairs_enabled())
    WORLD_FIELDS(
        P::action_payloads, "military_naval",
        naval_repair_codec::emit_action_payload_fields(
            out, w.common, w.naval.repairs, w.naval_repair_charges_enabled()));
  if (w.governance_tasks_enabled()) {
    auto registry =
        governance_task_payload_registry(w.common, w.tasks, w.governance_tasks);
    if (!registry.ok)
      return {false, {}, std::move(registry.error)};
    WORLD_FIELDS(P::split, "governance_tasks",
                 emit_governance_task_payloads_fields(out, registry.value));
  }
  if (w.envoy_tasks_enabled()) {
    DiplomaticMissionAccess missions(w.common, w.organization,
                                     *w.catalog->organization,
                                     *w.catalog->diplomacy, w.diplomacy);
    std::optional<DiplomaticReportAccess> reports;
    if (w.diplomacy.v2)
      reports.emplace(DiplomaticHistoryRead{w.common, w.organization,
                                            *w.catalog->organization,
                                            *w.catalog->diplomacy, w.diplomacy},
                      *w.diplomacy.v2);
    auto registry = envoy_task_payload_registry(
        w.common, w.tasks, w.envoy_tasks, *w.catalog->common, missions,
        missions, reports ? &*reports : nullptr);
    if (!registry.ok)
      return {false, {}, std::move(registry.error)};
    WORLD_FIELDS(P::split, "envoy_tasks",
                 emit_envoy_task_payloads_fields(out, registry.value));
  }
#include "resident/domains/tasks/world_codec/export_work_action_payloads.inc"
  for (const auto &row : w.common.state().actions.rows()) {
    const auto &core = row.second;
    bool exact_military = false;
    if (w.military_action_time) {
      auto owned = military_action_time_owns_active_action(*w.military_action_time, core);
      if (!owned.ok)
        return {false, {}, std::move(owned.error)};
      exact_military = owned.value;
    }
    const bool empty = exact_military ||
        core.kind == "wait" ||
        (w.family_birth_enabled() && core.kind == "family_child_care" &&
         w.family_birth.care_starts.find(row.first)) ||
        (w.family_care_relief_enabled() &&
         core.kind == "family_child_care_relief" &&
         w.child_care_relief.starts.find(row.first));
    auto applied = composition.adopt_action_payload(row.first, empty);
    if (!applied.ok)
      return applied;
  }
  auto inspection = validate_native_public_supply_inspection(w);
  if (!inspection.ok)
    return inspection;
  if (w.public_supply_inspection)
    WORLD_FIELDS(P::fact_payloads, "public_supply_inspection",
                 emit_public_supply_inspection_fact_payloads_fields(
                     out, *w.public_supply_inspection));
  // Field 全部原拥有头在原绑定遍历前登记，完整业务门借真实 World owner。
  auto field_fact_layout = validate_field_battle_layout(
      w.catalog->identity, w.catalog->field_battle.get(),
      w.field_battle ? &*w.field_battle : nullptr);
  if (!field_fact_layout.ok)
    return field_fact_layout;
  if (w.field_battle) {
    auto field_fact_sources = validate_native_field_world_sources(w);
    if (!field_fact_sources.ok)
      return field_fact_sources;
    WORLD_FIELDS(P::fact_payloads, "field_battle",
                 emit_field_battle_fact_payloads_fields(out, *w.field_battle));
  }
  // Strict borrowed application headers precede the sole global binding pass.
  // Their original receipt body remains only in character_condition_extensions.
  WORLD_FIELDS(P::fact_payloads, "actor_foundation",
               emit_native_condition_application_headers(
                   out, w.common, *w.catalog,
                   w.actor_foundation ? &*w.actor_foundation : nullptr));
  auto bound_facts = bind_world_export_facts(
      w, [&](const StableId &id) { return composition.has_fact_payload(id); },
      [&](const StableId &id, const StableId &domain) {
        composition.adopt_fact_domain(id, domain);
      },
      [&](const StableId &id) { composition.adopt_fact_payload(id); },
      [&](const FactRecord &header) -> Status {
        return composition.fields(
            P::validation_only, "private_shop_daily_rent", [&](auto &out) {
              return emit_private_shop_daily_rent_fact_payload_fields(
                  out, w.common, w.private_shop_daily_rent, header);
            });
      });
  if (!bound_facts.ok)
    return bound_facts;
  auto battles = composition.finish_battles();
  if (!battles.ok)
    return battles;
  if (w.dynamic_enabled()) {
#include "resident/domains/local_economy/world_codec/export_original_obligations.inc"
#include "resident/domains/organization/world_codec/export_runtime.inc"
    WORLD_FIELDS(P::section, "affairs",
                 emit_affairs_state_fields(out, w.affairs));
    WORLD_WHOLE(
        P::section, "director",
        emit_native_director_projection(out, "", w.director, *w.catalog));
    WORLD_FIELDS(P::section, "dynamic_tasks",
                 emit_dynamic_task_state_fields(out, w.tasks));
    if (!w.social.dynamic)
      fail("MissingDynamicSocial", "dynamic_social");
  }
  if (w.population_military_enabled())
    WORLD_FIELDS(P::section, "population_military",
                 emit_population_military_state_fields(
                     out, w.population_military, population_version(*w.catalog),
                     w.siege_integration_enabled()));
  if (w.siege_integration_enabled()) {
    WORLD_FIELDS(
        P::section, "siege_integration",
        emit_siege_integration_state_fields(out, w.military.siege_integration));
    WORLD_WHOLE(
        P::section, "military_integration",
        emit_military_integration_facts(out, "", w.military.integration_facts));
  }
  if (w.player_plan_enabled())
    WORLD_FIELDS(P::section, "player_plan",
                 emit_player_plan_state_fields(out, w.player_plan));
  if (w.diplomacy_enabled())
    WORLD_FIELDS(P::section, "diplomacy",
                 emit_diplomacy_state_fields(out, w.diplomacy));
  if (w.treaty_passage_enabled())
    WORLD_FIELDS(P::section, "treaty_passage",
                 emit_treaty_passage_state_fields(out, w.treaty_passage));
  if (w.political_military_appointments_enabled())
    WORLD_FIELDS(P::section, military_officer_political_module,
                 emit_political_military_appointments_fields(
                     out, w.political_military_appointments));
  if (w.political_field_appointments_enabled())
    WORLD_FIELDS(P::section, military_field_command_political_module,
                 emit_political_field_appointments_fields(
                     out, w.political_field_appointments));
  if (w.politics_enabled()) {
    if (political_body.value == 2)
      WORLD_WHOLE(P::section, "politics",
                  emit_politics2_projection(out, "", w.politics));
    else
      WORLD_FIELDS(P::section, "politics",
                   emit_politics_state_fields(out, w.politics));
  }
  if (w.political_fiscal_enabled()) {
    if (fiscal_body.value == 2)
      WORLD_WHOLE(
          P::section, "political_fiscal",
          emit_political_fiscal_state2_projection(out, "", w.political_fiscal));
    else
      WORLD_FIELDS(P::section, "political_fiscal",
                   emit_political_fiscal_state_fields(out, w.political_fiscal));
  }
  if (w.occupation_fiscal_enabled()) {
    WORLD_FIELDS(P::section, "political_occupation_fiscal",
                 emit_occupation_fiscal_state_fields(out, w.occupation_fiscal));
    WORLD_FIELDS(P::section, "occupation_fiscal_settlement",
                 emit_occupation_fiscal_settlement_fields(
                     out, w.occupation_fiscal_settlement));
  }
  if (w.political_takeover_enabled()) {
    if (!w.tasks.takeover_execution)
      fail("MissingTaskTakeoverExecutionState", "task_takeover_execution");
    if (takeover_body.value == 2)
      WORLD_WHOLE(P::section, "political_takeover",
                  emit_political_takeover_state2_projection(
                      out, "", w.political_takeover));
    else
      WORLD_FIELDS(
          P::section, "political_takeover",
          emit_political_takeover_state_fields(out, w.political_takeover));
    WORLD_FIELDS(P::section, "economic_takeover_execution",
                 emit_economic_takeover_execution_fields(
                     out, w.economic_takeover_execution));
    WORLD_FIELDS(P::section, "task_takeover_execution",
                 emit_task_public_account_execution_fields(
                     out, *w.tasks.takeover_execution));
  }
  if (w.fiscal_settlement_enabled())
    WORLD_FIELDS(P::section, "fiscal_settlement",
                 emit_fiscal_settlement_state_fields(out, w.fiscal_settlement));
  if (w.fiscal_taxpayer_continuity_enabled())
    WORLD_FIELDS(P::section, "fiscal_taxpayer_continuity",
                 emit_fiscal_taxpayer_continuity_fields(
                     out, w.fiscal_taxpayer_continuity));
  if (household_provision_management_enabled(w))
    WORLD_FIELDS(P::section, "household_provision_management",
                 emit_household_provision_management_state_fields(
                     out, w.household_provision_management));
  if (w.production_disruptions_enabled())
    WORLD_FIELDS(
        P::section, "production_disruptions",
        emit_production_disruption_state_fields(out, w.production_disruptions));
  if (w.family_life_enabled()) {
    if (w.catalog->family_life->selected_version == 4) {
      WORLD_WHOLE(P::section, "family_life",
                  emit_family_life4_projection(out, "", w.family_life));
    } else
      WORLD_FIELDS(P::section, "family_life",
                   emit_family_deaths_fields(out, w.family_life));
  }
  if (w.family_legacy_enabled())
    WORLD_FIELDS(P::section, "family_legacy",
                 emit_family_legacy_fields(out, w.family_legacy,
                                           w.catalog->family_legacy->version));
  if (w.family_bequest_enabled())
    WORLD_FIELDS(P::section, "family_bequest",
                 emit_family_bequests_fields(
                     out, w.family_bequest,
                     w.catalog->family_bequest->selected_version));
  if (w.family_inheritance_enabled())
    WORLD_FIELDS(P::section, "family_inheritance",
                 emit_family_inheritance_fields(
                     out, w.family_inheritance,
                     w.catalog->family_inheritance->selected_version));
  if (w.family_marriage_enabled())
    WORLD_FIELDS(P::section, "family_marriage",
                 emit_family_marriage_fields(out, w.family_marriage));
  if (w.family_birth_enabled())
    WORLD_FIELDS(
        P::section, "family_birth",
        emit_family_birth_fields(out, w.family_birth,
                                 w.catalog->family_birth->selected_version));
  if (w.family_care_relief_enabled())
    WORLD_FIELDS(P::section, "family_care_relief",
                 emit_child_care_relief_fields(out, w.child_care_relief));
  if (w.personal_recovery_enabled())
    WORLD_FIELDS(P::section, "personal_injury_recovery",
                 emit_growth_recovery_state_fields(out, w.personal_recovery));
  if (w.catalog->interaction->visit_messages) {
    if (!w.interaction.visit_messages)
      fail("MissingVisitMessageDomain", "visit_messages");
    WORLD_WHOLE(
        P::section, "visit_messages",
        emit_visit_messages_projection(out, "", *w.interaction.visit_messages));
  }
  if (w.catalog->director && w.catalog->director->visit_reception) {
    if (!w.interaction.visit_reception_claims)
      fail("MissingVisitReceptionClaims", "visit_reception_claims");
    WORLD_FIELDS(P::section, "visit_reception_claims",
                 emit_visit_reception_claims_fields(
                     out, *w.interaction.visit_reception_claims));
  }
  if (w.estate_employment_enabled())
    WORLD_FIELDS(P::section, "estate_employment",
                 emit_estate_employment_state_fields(out, w.estate_employment));
  if (w.economic_supply_policies_enabled()) {
    WORLD_FIELDS(P::section, "economic_supply_policies",
                 emit_economic_supply_policy_state_fields(
                     out, w.economic_supply_policies));
    WORLD_FIELDS(P::section, "political_supply_adoptions",
                 emit_political_supply_adoptions_fields(
                     out, w.political_supply_adoptions));
  }
  if (w.politics_governance_enabled())
    WORLD_FIELDS(
        P::section, "politics_governance",
        emit_politics_governance_state_fields(out, w.politics_governance));
  if (w.governance_tasks_enabled())
    WORLD_FIELDS(P::section, "governance_tasks",
                 emit_governance_task_state_fields(out, w.governance_tasks));
  if (w.public_relief_enabled())
    WORLD_FIELDS(P::section, "public_relief",
                 emit_public_relief_state_fields(out, w.public_relief));
  if (w.catalog->task_personal_advances) {
    if (!w.tasks.personal_advances)
      fail("MissingTaskAdvanceDomain", "task_personal_advances");
    WORLD_FIELDS(
        P::section, "task_personal_advances",
        emit_task_personal_advances_fields(out, *w.tasks.personal_advances));
  }
  if (w.catalog->task_message_handoffs_enabled) {
    if (!w.tasks.message_handoffs)
      fail("MissingTaskMessageHandoffDomain", "task_message_handoffs");
    WORLD_FIELDS(
        P::section, "task_message_handoffs",
        emit_task_message_handoffs_fields(out, *w.tasks.message_handoffs));
  }
  if (w.catalog->task_message_notices) {
    if (!w.tasks.message_notices)
      fail("MissingTaskMessageNoticeDomain", "task_message_notices");
    WORLD_FIELDS(
        P::section, "task_message_notices",
        emit_task_message_notices_fields(out, *w.tasks.message_notices));
  }
  if (w.catalog->task_static_assets_enabled) {
    if (!w.tasks.static_assets)
      fail("MissingTaskStaticAssetDomain", "task_static_assets");
    WORLD_FIELDS(P::section, "task_static_assets",
                 emit_task_static_assets_fields(out, *w.tasks.static_assets));
  }
  if (w.catalog->task_extensions) {
    if (!w.tasks.deadline_extensions)
      fail("MissingTaskExtensionDomain", "task_extensions");
    WORLD_FIELDS(
        P::section, "task_extensions",
        emit_task_extensions_fields(out, *w.tasks.deadline_extensions));
  }
  if (w.catalog->personal_appointments) {
    if (!w.interaction.personal_appointments)
      fail("MissingPersonalAppointmentDomain", "personal_appointments");
    WORLD_FIELDS(P::section, "personal_appointments",
                 emit_personal_appointments_fields(
                     out, *w.interaction.personal_appointments));
  }
  if (w.catalog->task_offer_responses) {
    if (!w.tasks.offer_responses)
      fail("MissingTaskOfferResponseDomain", "task_offer_responses");
    WORLD_FIELDS(
        P::section, "task_offer_responses",
        emit_task_offer_response_fields(out, *w.tasks.offer_responses));
  }
  if (w.growth_market_history_enabled())
    WORLD_FIELDS(
        P::section, "growth_market_history",
        w.catalog->growth_market_history->selected_version == 2
            ? emit_growth_market_history_v2_fields(
                  out, w.growth_market_history,
                  w.catalog->growth_market_history->resource_codec)
            : emit_growth_market_history_fields(out, w.growth_market_history));
  if (w.political_shop_licensing_enabled()) {
    auto selected =
        selected_native_political_shop_licensing_version(w.catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (!selected.value || selected.value != w.political_shop_licensing.version)
      return common_failure(
          "PoliticalShopLicensingCheckpointSelectionMismatch");
    if (selected.value == 2) {
      WORLD_WHOLE(P::section, "political_shop_licensing",
                  emit_political_shop_licensing_projection2(
                      out, "", w.political_shop_licensing));
    } else {
      WORLD_FIELDS(P::section, "political_shop_licensing",
                   emit_political_shop_licensing_fields(
                       out, w.political_shop_licensing));
    }
  }
  if (w.political_shop_registry_successions_enabled())
    WORLD_FIELDS(P::section, "political_shop_registry_successions",
                 emit_political_shop_registry_successions_fields(
                     out, w.political_shop_registry_successions));
  if (w.catalog->political_shop_licensing_receipts) {
    if (!w.political_shop_licensing_receipts)
      fail("PoliticalShopLicensingReceiptsProfileMismatch", "state");
    WORLD_FIELDS(P::section, "political_shop_licensing_receipts",
                 emit_political_shop_licensing_receipts_fields(
                     out, *w.political_shop_licensing_receipts));
  }
  if (w.catalog->political_governance_continuations) {
    if (!w.political_governance_continuations)
      fail("GovernanceContinuationProfileRequired", "state");
    WORLD_FIELDS(P::section, "political_governance_continuations",
                 emit_political_governance_continuations_fields(
                     out, *w.political_governance_continuations));
  }
  if (w.catalog->political_governance_invitation_replies) {
    if (!w.governance_invitation_replies)
      fail("GovernanceInvitationProfileRequired", "state");
    WORLD_FIELDS(P::section, "political_governance_invitation_replies",
                 emit_governance_invitation_replies_fields(
                     out, *w.governance_invitation_replies));
  }
  {
    auto checked = validate_private_shop_daily_rent_layout(
        w.common, w.private_shop_daily_rent_enabled()
                      ? &w.private_shop_daily_rent
                      : nullptr);
    if (!checked.ok)
      return checked;
    if (w.private_shop_daily_rent_enabled())
      WORLD_FIELDS(P::section, private_shop_daily_rent_module,
                   emit_private_shop_daily_rent_state_fields(
                       out, w.private_shop_daily_rent));
  }
  if (w.private_shops_enabled())
    WORLD_FIELDS(P::section, "private_shops",
                 emit_private_shop_state_fields(out, w.private_shops));
  if (w.task_shop_orders_enabled()) {
    if (!w.tasks.shop_orders)
      fail("MissingTaskShopOrderDomain", "task_shop_orders");
    WORLD_FIELDS(P::section, "task_shop_orders",
                 emit_task_shop_orders_fields(out, *w.tasks.shop_orders));
  }
  if (w.private_shop_liquidations_enabled()) {
    if (!w.tasks.shop_orders || !w.tasks.shop_orders->estate_cancellations)
      fail("TaskShopOrderEstateProfileMismatch",
           "task_shop_order_estate_cancellations");
    WORLD_FIELDS(P::section, "private_shop_liquidations",
                 emit_private_shop_liquidations_fields(
                     out, w.private_shop_liquidations));
    WORLD_FIELDS(P::section, "task_shop_order_estate_cancellations",
                 emit_task_shop_order_estate_cancellations_fields(
                     out, *w.tasks.shop_orders->estate_cancellations));
  }
  if (w.catalog->family_apprenticeship)
    WORLD_FIELDS(
        P::section, "family_apprenticeship",
        emit_family_apprenticeship_fields(out, w.family_apprenticeship));
  if (w.catalog->contact_inquiries) {
    if (!w.interaction.contact_inquiries)
      fail("MissingContactInquiryDomain", "contact_inquiries");
    WORLD_FIELDS(
        P::section, "contact_inquiries",
        emit_contact_inquiries_fields(out, *w.interaction.contact_inquiries));
  }
  if (w.catalog->visit_message_replies) {
    if (!w.interaction.visit_message_replies)
      fail("MissingVisitReplyDomain", "visit_message_replies");
    WORLD_FIELDS(
        P::section, "visit_message_replies",
        emit_visit_replies_fields(out, *w.interaction.visit_message_replies));
  }
  if (w.catalog->family_disciple_legacy)
    WORLD_FIELDS(
        P::section, "family_disciple_legacy",
        emit_disciple_control_state_fields(out, w.family_disciple_legacy));
  if (w.catalog->born_personal_dialogues) {
    if (!w.interaction.born_personal_dialogues)
      fail("MissingBornPersonalDialogueDomain", "born_personal_dialogues");
    WORLD_FIELDS(P::section, "born_personal_dialogues",
                 emit_born_personal_dialogues_fields(
                     out, *w.interaction.born_personal_dialogues));
  }
  if (w.catalog->family_biological_legacy)
    WORLD_FIELDS(
        P::section, "family_biological_legacy",
        emit_biological_control_state_fields(out, w.family_biological_legacy));
  if (w.catalog->political_successions) {
    if (!w.political_successions)
      fail("PoliticalSuccessionProfileRequired", "state");
    WORLD_FIELDS(
        P::section, political_succession_domain,
        emit_political_succession_state_fields(out, *w.political_successions));
  }
  if (w.catalog->visit_appointment_invitations) {
    if (!w.interaction.visit_appointment_invitations ||
        !w.interaction.visit_appointment_meetings)
      fail("MissingVisitAppointmentDomain", "state");
    WORLD_FIELDS(P::section, "visit_appointment_invitations",
                 emit_visit_appointments_fields(
                     out, *w.interaction.visit_appointment_invitations));
    WORLD_FIELDS(P::section, "visit_appointment_meetings",
                 emit_visit_appointment_meetings_fields(
                     out, *w.interaction.visit_appointment_meetings));
  }
  if (w.catalog->political_head_handovers) {
    if (!w.political_head_handovers)
      fail("PoliticalHeadHandoverProfileRequired", "state");
    WORLD_FIELDS(P::section, political_head_handover_domain,
                 emit_political_head_handover_state_fields(
                     out, *w.political_head_handovers));
  }
  if (w.growth_tactic_lessons_enabled())
    WORLD_FIELDS(
        P::section, "growth_tactic_lessons",
        emit_growth_tactic_lessons_fields(out, w.growth_tactic_lessons));
  if (w.catalog->political_successor_rounds) {
    if (!w.political_successor_rounds)
      fail("PoliticalSuccessorRoundProfileRequired", "state");
    WORLD_FIELDS(P::section, political_successor_rounds_domain,
                 emit_political_successor_round_state_fields(
                     out, *w.political_successor_rounds));
  }
  if (w.private_shop_command_results_enabled())
    WORLD_FIELDS(P::section, private_shop_command_results_module,
                 emit_private_shop_command_results_state_fields(
                     out, w.private_shop_command_results));
  if (w.catalog->political_term_handovers) {
    if (!w.political_term_handovers)
      fail("PoliticalTermHandoverProfileRequired", "state");
    WORLD_FIELDS(P::section, political_term_handover_domain,
                 emit_political_term_handover_state_fields(
                     out, *w.political_term_handovers));
  }
  if (w.catalog->visit_message_receipts_enabled) {
    if (!w.interaction.visit_message_receipts)
      fail("MissingVisitMessageReceiptDomain", visit_message_receipt_module);
    WORLD_WHOLE(P::section, visit_message_receipt_module,
                emit_visit_message_receipts_projection(
                    out, "", *w.interaction.visit_message_receipts));
  }
  if (w.catalog->known_personal_dialogues) {
    if (!w.interaction.known_personal_dialogues)
      fail("MissingKnownPersonalDialogueDomain", "known_personal_dialogues");
    WORLD_FIELDS(P::section, "known_personal_dialogues",
                 emit_known_personal_dialogues_fields(
                     out, *w.interaction.known_personal_dialogues));
  }
  if (w.catalog->political_office_receptions) {
    if (!w.political_office_receptions)
      fail("PoliticalOfficeReceptionProfileRequired", "state");
    WORLD_FIELDS(P::section, political_office_reception_domain,
                 emit_political_office_reception_state_fields(
                     out, *w.political_office_receptions));
  }
  if (w.catalog->political_office_resignations) {
    if (!w.political_office_resignations)
      fail("PoliticalOfficeResignationProfileRequired", "state");
    WORLD_FIELDS(P::section, political_office_resignation_module,
                 emit_political_office_resignation_state_fields(
                     out, *w.political_office_resignations));
  }
  if (w.catalog->political_subordinate_appointments) {
    if (!w.political_subordinate_appointments)
      fail("PoliticalSubordinateAppointmentProfileRequired", "state");
    WORLD_FIELDS(P::section, political_subordinate_appointment_module,
                 emit_political_subordinate_appointment_state_fields(
                     out, *w.political_subordinate_appointments));
  }
  if (w.catalog->political_dismissal_sources) {
    if (!w.political_dismissal_sources)
      fail("PoliticalDismissalSourceProfileRequired", "state");
    WORLD_FIELDS(P::section, political_dismissal_sources_module,
                 emit_political_dismissal_sources_fields(
                     out, *w.political_dismissal_sources));
  }
  if (w.catalog->political_governance_agencies) {
    if (!w.political_governance_agencies)
      fail("PoliticalGovernanceAgencyProfileRequired", "state");
    WORLD_FIELDS(P::section, political_governance_agency_domain,
                 emit_political_governance_agency_state_fields(
                     out, *w.political_governance_agencies));
  }
  if (w.catalog->task_shop_order_contacts_enabled) {
    if (!w.tasks.shop_orders || !w.tasks.shop_orders->contacts)
      fail("MissingTaskShopOrderContactDomain", task_shop_order_contact_module);
    WORLD_FIELDS(P::section, task_shop_order_contact_module,
                 emit_task_shop_order_contacts_fields(
                     out, *w.tasks.shop_orders->contacts));
  }
  if (w.catalog->family_collateral_legacy)
    WORLD_FIELDS(
        P::section, "family_collateral_legacy",
        emit_collateral_control_state_fields(out, w.family_collateral_legacy));
  if (w.catalog->task_message_returns) {
    if (!w.tasks.message_returns)
      fail("MissingTaskMessageReturnDomain", task_message_return_module);
    WORLD_FIELDS(
        P::section, task_message_return_module,
        emit_task_message_returns_fields(out, *w.tasks.message_returns));
  }
  if (w.catalog->family_voluntary_paths)
    WORLD_FIELDS(
        P::section, "family_voluntary_paths",
        emit_voluntary_path_state_fields(out, w.family_voluntary_paths));
  if (w.catalog->task_personal_messages) {
    if (!w.tasks.personal_messages)
      fail("MissingTaskPersonalMessageDomain", task_personal_message_module);
    WORLD_FIELDS(
        P::section, task_personal_message_module,
        emit_task_personal_messages_fields(out, *w.tasks.personal_messages));
  }
  if (w.catalog->task_personal_reports) {
    if (!w.tasks.personal_reports)
      fail("MissingTaskPersonalReportDomain", task_personal_report_module);
    WORLD_FIELDS(
        P::section, task_personal_report_module,
        emit_task_personal_reports_fields(out, *w.tasks.personal_reports));
  }
  if (w.catalog->task_personal_acceptances) {
    if (!w.tasks.personal_acceptances)
      fail("MissingTaskPersonalAcceptanceDomain",
           task_personal_acceptance_module);
    WORLD_FIELDS(P::section, task_personal_acceptance_module,
                 emit_task_personal_acceptances_fields(
                     out, *w.tasks.personal_acceptances));
  }
  if (w.catalog->task_personal_cancellations) {
    if (!w.tasks.personal_cancellations)
      fail("MissingTaskPersonalCancellationDomain",
           task_personal_cancellation_module);
    WORLD_FIELDS(P::section, task_personal_cancellation_module,
                 emit_task_personal_cancellations_fields(
                     out, *w.tasks.personal_cancellations));
  }
  if (w.envoy_tasks_enabled())
    WORLD_FIELDS(P::section, "envoy_tasks",
                 emit_envoy_task_state_fields(out, w.envoy_tasks));
  if (w.naval_enabled())
    WORLD_FIELDS(P::section, "military_naval",
                 naval_codec::emit_fields(out, w.naval, *w.catalog->naval));
  if (w.military_tactic_learning_enabled())
    WORLD_FIELDS(P::section, military_tactic_learning_module,
                 military_tactic_learning_codec::emit_fields(
                     out, w.military_tactic_learning));
#include "resident/domains/actor_foundation/world_codec/export_exact_origins.inc"
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    if (!w.facility_claims)
      return common_failure("MissingFacilityClaims");
    WORLD_WHOLE(P::section, "facility_claims",
                emit_facility_claims2_projection(out, "", *w.facility_claims));
  } else if (w.facility_claims)
    return common_failure("UnexpectedFacilityClaims");
  // JSON与二进制共用同一原选择和字段表，未选残留不能导出。
  auto population_version =
      selected_native_population_transport_version(w.catalog->identity);
  if (!population_version.ok)
    return {false, {}, std::move(population_version.error)};
  if (bool(population_version.value) != bool(w.population_transport))
    return common_failure("PopulationTransportSelectedStateMismatch");
  if (population_version.value)
    WORLD_WHOLE(P::section, "civilian_transport_population",
                emit_population_transport(out, *w.population_transport,
                                          bool(w.catalog->household_purchases),
                                          population_version.value));
  if (w.catalog->household_purchases) {
    if (!w.household_purchases || !w.household_retail_settlements)
      return common_failure("MissingHouseholdSelectedState");
    WORLD_WHOLE(
        P::section, "household_purchases",
        emit_household_purchases(out, *w.household_purchases,
                                 w.catalog->selected_household_version));
    WORLD_WHOLE(
        P::section, "household_retail_settlements",
        emit_household_retail_state(out, *w.household_retail_settlements));
  } else if (w.household_purchases || w.household_retail_settlements)
    return common_failure("UnexpectedHouseholdState");
  if (w.catalog->exact_time &&
      (exact_social_selected(*w.catalog->exact_time) ||
       exact_complete_selected(*w.catalog->exact_time))) {
    if (!w.social_period_sources)
      return common_failure("MissingSocialPeriodSources");
    WORLD_FIELDS(
        P::section, "social_period_sources",
        emit_social_period_sources_fields(out, *w.social_period_sources));
  }
  auto face_sources = validate_native_face_to_face_world(w);
  if (!face_sources.ok)
    return face_sources;
  if (w.interaction.face_to_face_contacts)
    WORLD_WHOLE(P::section, "face_to_face_contacts",
                emit_face_to_face_contact_sources_projection(
                    out, "", *w.interaction.face_to_face_contacts));
  auto occupancy = validate_native_actor_social_occupancy(w);
  if (!occupancy.ok)
    return occupancy;
  if (w.catalog->actor_social_occupancy_selected)
    WORLD_WHOLE(P::section, "actor_social_occupancy",
                emit_actor_social_occupancy_projection(
                    out, "", *w.interaction.social_occupancy));
  auto life_age = validate_native_life_age_layout(w);
  if (!life_age.ok)
    return life_age;
  if (w.catalog->life_age_checks)
    WORLD_WHOLE(P::section, "life_age_checks",
                emit_life_age_checks_projection(out, "", *w.life_age_checks));
  if (w.catalog->control_continuation)
    WORLD_WHOLE(P::section, "control_continuation",
                w.catalog->control_continuation->selected_version == 3
                    ? emit_control_continuation_v3_projection(
                          out, "", *w.control_continuation)
                    : emit_control_continuation_v2_projection(
                          out, "", *w.control_continuation));
  auto organization_layout = validate_native_organization_runtime_layout(w);
  if (!organization_layout.ok)
    return organization_layout;
  auto obligation_layout = validate_native_economic_obligation_layout(w);
  if (!obligation_layout.ok)
    return obligation_layout;
  auto birth_layout = validate_native_family_birth_layout(w);
  if (!birth_layout.ok)
    return birth_layout;
  auto population_layout = validate_native_born_population_layout(w);
  if (!population_layout.ok)
    return population_layout;
  auto governance_layout = validate_native_governance_program_layout(w);
  if (!governance_layout.ok)
    return governance_layout;
  if (w.governance_programs)
    WORLD_WHOLE(P::section, "governance_programs",
                emit_native_governance_programs_projection(
                    out, "", *w.governance_programs,
                    w.governance_programs->schema_version));
#include "resident/domains/craft/world_codec/export_item_craft_state.inc"
#include "resident/domains/local_economy/world_codec/export_facility_state.inc"
#include "resident/domains/military/world_codec/export_exact_actions.inc"
#include "resident/domains/tasks/world_codec/export_exact_execution.inc"
  auto field_passage = validate_native_field_passage_layout(w);
  if (!field_passage.ok) return field_passage;
  if (w.political_passage)
    WORLD_WHOLE(P::section, "political_passage", emit_field_passage_projection(
        out, "", *w.political_passage, *w.political_safe_exit));
  auto relations_state = validate_native_political_relations_world(w);
  if (!relations_state.ok)
    return relations_state;
  if (w.political_relations)
    WORLD_WHOLE(
        P::section, "political_relations",
        emit_political_relations_projection(out, "", *w.political_relations));
  if (w.public_supply_inspection)
    WORLD_FIELDS(
        P::section, "public_supply_inspection",
        emit_public_supply_inspection_state(out, *w.public_supply_inspection));
#include "resident/domains/commerce/world_codec/export_shop_operations.inc"
#include "resident/domains/commerce/world_codec/export_transport_funding.inc"
#include "resident/domains/local_economy/world_codec/export_private_shop_custody.inc"
#include "resident/domains/local_economy/world_codec/export_production_cycles.inc"
#include "resident/domains/tasks/world_codec/export_work.inc"
#include "resident/domains/travel/world_codec/export_physical_sections.inc"
#undef WORLD_FIELDS
#undef WORLD_WHOLE
  return composition.finish();
}
} // namespace
Outcome<BoundaryValue> export_world_state(const WorldBundle &w) {
  try {
    TreeWorldExportComposition composition(w.common.state());
    auto emitted = emit_world_export_components(w, composition);
    if (!emitted.ok)
      return {false, {}, std::move(emitted.error)};
    return {true, std::move(composition).take(), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status validate_world_export_shape(const WorldBundle &w,
                                   const CapacityProfile &capacity,
                                   CheckpointJobBudget &job) {
  try {
    auto profile = job.check_profile(capacity);
    if (!profile.ok)
      throw checkpoint_job_detail::FailureException{std::move(profile.error)};
    auto memory = checkpoint_memory_resource(job, CheckpointJobUse::scratch);
    if (!memory.ok)
      throw checkpoint_job_detail::FailureException{std::move(memory.error)};
    ScopedCheckpointMemory scope(memory.value);
    ShapeWorldExportComposition composition(w.common.state(), capacity, job,
                                            memory.value);
    return emit_world_export_components(w, composition);
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
