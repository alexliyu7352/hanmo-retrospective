// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
// 两种输入运输复用唯一typed域派发；字段codec、精确版本及跨域seed校验不复制第二套。
struct WorldCheckpointDecoder {
  template <class Seed, class T> static auto make_state() {
    if constexpr (std::is_same_v<Seed, DecodedWorldSeedOwner>)
      return checkpoint_make_unique<T>();
    else
      return std::make_shared<T>();
  }
  template <class Seed, class Index, class ReaderFactory>
  static Outcome<Seed>
  decode(const Index &index, const PreparedCatalog &catalog,
         CodecDecodeBudget &budget, ReaderFactory make_reader) {
    Seed seed;
    ResidentResultCodec receipts;
    bool organization_body_decoded = false,
         organization_effects_decoded = false, personal_body_decoded = false,
         personal_lifecycle_decoded = false, military_body_decoded = false,
         military_lifecycle_decoded = false;
    auto exact_timeline = make_state<Seed, WorldTimelineState>();
    auto exact_elapsed = make_state<Seed, ActionElapsedStore>();
    auto actor_foundation = make_state<Seed, AuthoredActorFoundationState>();
    auto injury_version =
        selected_native_character_injury_source_version(catalog.identity);
    if (!injury_version.ok)
      return {false, {}, std::move(injury_version.error)};
    decltype(make_state<Seed, CharacterInjurySourcesStateV1>()) injury_sources;

#include "resident/domains/politics/world_codec/checkpoint_read_version_guards.inc"
    auto t = make_state<Seed, TravelState>();
    auto m = make_state<Seed, MilitaryState>();
    auto i = make_state<Seed, InteractionState>();
    auto s = make_state<Seed, SocialState>();
    auto e = make_state<Seed, EstateState>();
    auto tasks = make_state<Seed, TaskState>();
    auto family = make_state<Seed, FamilyState>();
    auto personal = make_state<Seed, PersonalState>();
    auto growth = make_state<Seed, GrowthState>();
    const bool terminal_selected =
        catalog.exact_time && exact_complete_selected(*catalog.exact_time);
    bool terminal_decoded = false, growth_body_decoded = false;
    // 独立段先收进本候选真表；全部旧 Growth 恢复结束后只 move 一次，避免覆盖。
    OrderedTable<GrowthTerminalPublication> terminal_publications;
    const bool trade_grants_selected =
        catalog.growth->complete_capability_application_chain;
    bool trade_grants_decoded = false;
    OrderedTable<GrowthTradeGrant> trade_grants;
    auto household_management =
        make_state<Seed, HouseholdProvisionManagementState>();
    auto disruption = make_state<Seed, ProductionDisruptionState>();
    auto life = make_state<Seed, FamilyDeathState>();
    auto legacy = make_state<Seed, FamilyLegacyState>();
    auto bequest = make_state<Seed, FamilyBequestState>();
    auto inheritance = make_state<Seed, FamilyInheritanceState>();
    auto marriage = make_state<Seed, FamilyMarriageState>();
    auto birth = make_state<Seed, FamilyBirthState>();
    auto care_relief = make_state<Seed, ChildCareReliefState>();
    auto recovery = make_state<Seed, GrowthRecoveryState>();
    decltype(make_state<Seed, MilitaryActionTimeState>()) military_action_time;
    decltype(make_state<Seed, FieldBattleWorldState>()) field_battle;
    decltype(make_state<Seed, PoliticalRelationsState>()) political_relations;
    decltype(make_state<Seed, PoliticalPassageDecisionState>()) political_passage;
    decltype(make_state<Seed, SafeExitState>()) political_safe_exit;
    decltype(make_state<Seed, PublicSupplyInspectionState>())
        public_supply_inspection;
    auto economy = make_state<Seed, LocalEconomyState>();
    auto organization = make_state<Seed, OrganizationState>();
    auto affairs = make_state<Seed, AffairsState>();
    auto director = make_state<Seed, DirectorState>();
    auto population = make_state<Seed, PopulationMilitaryState>();
    auto plan = make_state<Seed, PlayerPlanState>();
    auto politics = make_state<Seed, PoliticsState>();
    auto fiscal = make_state<Seed, PoliticalFiscalState>();
    auto settlement = make_state<Seed, FiscalSettlementState>();
    auto relief = make_state<Seed, PublicReliefState>();
    auto boundary_version =
        selected_native_movement_boundary_version(catalog.identity);
    if (!boundary_version.ok)
      return {false, {}, std::move(boundary_version.error)};
    bool boundary_decoded = false;
    auto continuation_version =
        selected_native_movement_boundary_continuation_version(catalog.identity);
    if (!continuation_version.ok)
      return {false, {}, std::move(continuation_version.error)};
    bool continuation_decoded = false;
    auto local_move_version =
        selected_native_local_move_version(catalog.identity);
    if (!local_move_version.ok)
      return {false, {}, std::move(local_move_version.error)};
    bool local_move_decoded = false;
    auto handoff_versions =
        selected_native_group_member_handoff_versions(catalog.identity);
    if (!handoff_versions.ok)
      return {false, {}, std::move(handoff_versions.error)};
    bool member_releases_decoded = false, member_losses_decoded = false;
    auto transport_versions =
        selected_native_transport_versions(catalog.identity);
    if (!transport_versions.ok)
      return {false, {}, std::move(transport_versions.error)};
    decltype(make_state<Seed, MovementExactState>()) movement;
    decltype(make_state<Seed, SpatialCarrierState>()) carriers;
    decltype(make_state<Seed, GroupMovementExecutionState>()) executions;
    decltype(make_state<Seed, TransportRegistrationState>()) registrations;
    decltype(make_state<Seed, EconomicTransportExpenseState>()) payments;
    std::optional<CommonStore> transport_common;
    NativeMovementBasisDecodeAllocation basis_allocation(budget);
    std::unique_ptr<TransportPhysicalCodecContext> transport_context;
    if (transport_versions.value.travel) {
      movement = make_state<Seed, MovementExactState>();
      carriers = make_state<Seed, SpatialCarrierState>();
      executions = make_state<Seed, GroupMovementExecutionState>();
      registrations = make_state<Seed, TransportRegistrationState>();
      payments = make_state<Seed, EconomicTransportExpenseState>();
    }
    // 封套目录保持其原UTF-8排序；只改变私有typed恢复的借用顺序，无载荷副本。
    using Section = std::decay_t<decltype(*index.sections.begin())>;
    CheckpointVector<const Section *> restore_order;
    restore_order.reserve(index.sections.size());
    if (transport_versions.value.travel)
      for (const auto *name : {"common", "travel", "group_transport",
                               "transport_manifest_bindings"})
        for (const auto &section : index.sections)
          if (section.domain == name)
            restore_order.push_back(&section);
    // The envelope retains its canonical UTF-8 order. Only the private typed
    // reader visits the actual owner before adjuncts whose names sort earlier.
    for (const auto &section : index.sections)
      if (section.domain == "military_action_time")
        restore_order.push_back(&section);
    for (const auto &section : index.sections) {
      const bool transport_first =
          transport_versions.value.travel &&
          (section.domain == "common" || section.domain == "travel" ||
           section.domain == "group_transport" ||
           section.domain == "transport_manifest_bindings");
      if (!transport_first && section.domain != "military_action_time")
        restore_order.push_back(&section);
    }
    // 动态任务附项反读依赖同一 TaskState 的原任务/动作表；只调整私有借读顺序。
    // 封套与写入字段不动，已在附项之前的原 owner 保持原位，缺 owner
    // 仍由原校验拒绝。
    const auto task_owner = std::find_if(
        restore_order.begin(), restore_order.end(),
        [](const Section *part) { return part->domain == "tasks"; });
    const auto task_extensions = std::find_if(
        restore_order.begin(), restore_order.end(),
        [](const Section *part) { return part->domain == "dynamic_tasks"; });
    if (task_owner != restore_order.end() &&
        task_extensions != restore_order.end() && task_extensions < task_owner)
      std::rotate(task_extensions, task_owner, task_owner + 1);
    bool common_read = false, transport_finished = false;
    for (const auto *section_ref : restore_order) {
      const auto &section = *section_ref;
      CodecVisitor reader = make_reader(section);
      auto resource_features =
          configure_domain_resource_references(reader, catalog.identity);
      if (!resource_features.ok)
        return {false, {}, std::move(resource_features.error)};
      reader.political_head_handover_codec_version = handover_version.value;
      reader.political_term_handover_codec_version = term_version.value;
      reader.political_office_reception_codec_version = reception_version.value;
      reader.political_office_resignation_codec_version =
          resignation_version.value;
      reader.political_subordinate_appointment_codec_version =
          subordinate_version.value;
      reader.political_governance_agency_codec_version = agency_version.value;
      reader.political_successor_round_codec_version = round_version.value;
      reader.political_succession_codec_version = succession_version.value;
      if (section.domain == "military") {
        auto base = military_checkpoint_base_version(
            section.codec_version, catalog.selected_military_occupation_schema);
        if (!base.ok)
          return {false, {}, std::move(base.error)};
        if (base.value == 16 || base.value == 17)
          reader.military_codec_version = base.value;
      }
      if (catalog.private_shop_daily_rent)
        reader.schema_features |= private_shop_daily_rent_checkpoint_feature;
      if (catalog.political_governance_invitation_replies)
        reader.schema_features |= governance_invitation_reply_feature;
      if (catalog.political_governance_continuations)
        reader.schema_features |= political_governance_continuation_feature;
      if (catalog.private_shops && catalog.political_shop_licensing)
        reader.schema_features |= private_shop_checkpoint_feature;
      if (catalog.political_shop_registry_successions)
        reader.schema_features |=
            political_shop_registry_succession_checkpoint_feature;
      if (catalog.private_shop_liquidations)
        reader.schema_features |= private_shop_liquidation_checkpoint_feature;
      if (catalog.economic_supply_policies_enabled)
        reader.schema_features |= economic_supply_policy_checkpoint_feature;
      if (officers_enabled(catalog))
        reader.schema_features |= military_officer_checkpoint_feature;
      if (field_commands_enabled(catalog))
        reader.schema_features |= military_field_command_checkpoint_feature;
      if (military_recovery_enabled(catalog))
        reader.schema_features |= military_recovery_checkpoint_feature;
      if (military_supply_attrition_enabled(catalog))
        reader.schema_features |= military_supply_attrition_checkpoint_feature;
      if (military_rout_enabled(catalog))
        reader.schema_features |= military_rout_checkpoint_feature;
      if (military_command_shock_enabled(catalog))
        reader.schema_features |= military_command_shock_checkpoint_feature;
      if (military_garrison_rest_enabled(catalog))
        reader.schema_features |= military_garrison_rest_checkpoint_feature;
      if (military_combat_conditions_enabled(catalog))
        reader.schema_features |= military_combat_conditions_checkpoint_feature;
      if (military_commanded_damage_enabled(catalog))
        reader.schema_features |= military_commanded_damage_checkpoint_feature;
      if (military_command_resources_enabled(catalog))
        reader.schema_features |= military_command_resources_checkpoint_feature;
      if (treaty_passage_enabled(catalog))
        reader.schema_features |= treaty_passage_checkpoint_feature;
      if (occupation_fiscal_enabled(catalog))
        reader.schema_features |= occupation_fiscal_checkpoint_feature;
      if (catalog.diplomacy && catalog.diplomacy->version == 2)
        reader.schema_features |= diplomatic_report_checkpoint_feature;
      if (catalog.naval) {
        reader.schema_features |= naval_command_codec::checkpoint_feature;
        reader.naval_codec_version = naval_codec::version(*catalog.naval);
      }
      if (requests_enabled(catalog))
        reader.schema_features |= military_request_codec::checkpoint_feature;
      if (section.domain == "growth" &&
          catalog.growth->teaching_requests.enabled)
        reader.schema_features |= common_checkpoint::teaching_request_sources;
      if (section.domain == "growth" &&
          catalog.growth->study_teaching_requests.enabled)
        reader.schema_features |=
            common_checkpoint::study_teaching_request_sources;
      if (section.domain == "growth" && catalog.growth->innate_learning)
        reader.schema_features |= innate_study_checkpoint_feature;
      if (management_enabled(catalog))
        reader.schema_features |=
            common_checkpoint::production_configuration_receipts;
      if (household_management_enabled(catalog))
        reader.schema_features |=
            common_checkpoint::household_provision_configuration_receipts;
      if (politics_enabled(catalog))
        reader.schema_features |= common_checkpoint::politics_receipts;
      if (political_fiscal_enabled(catalog))
        reader.schema_features |= common_checkpoint::political_fiscal_receipts;
      if (catalog.dynamic_profile == DynamicProfile::local_society_v1)
        reader.schema_features |= common_checkpoint::typed_resource_references;
      if (catalog.named_membership)
        reader.schema_features |= common_checkpoint::named_membership_receipts;
      if (section.domain == "common") {
        auto physical = configure_common_physical_codec(
            reader, section.codec_version, catalog.identity);
        if (!physical.ok)
          return {false, {}, std::move(physical.error)};
      }
      Status r;
      if (section.domain == "common") {
        if (common_read)
          return {false,
                  {},
                  common_failure("DuplicateCommonCheckpointDomain").error};
        r = visit_common_checkpoint(reader, seed.common, &receipts);
        if (r.ok)
          common_read = true;
      }
#include "resident/domains/travel/world_codec/checkpoint_read_travel.inc"
#include "resident/domains/military/world_codec/checkpoint_read_military.inc"
#include "resident/domains/interaction/world_codec/checkpoint_read_interaction.inc"
#include "resident/domains/social/world_codec/checkpoint_read_social.inc"
#include "resident/domains/estate/world_codec/checkpoint_read_estate.inc"
#include "resident/domains/tasks/world_codec/checkpoint_read_tasks.inc"
#include "resident/domains/family/world_codec/checkpoint_read_family.inc"
#include "resident/domains/personal/world_codec/checkpoint_read_personal.inc"
#include "resident/domains/growth/world_codec/checkpoint_read_growth.inc"
      else if (section.domain == "trade-grant") {
        if (!trade_grants_selected || section.codec_version != 1 ||
            trade_grants_decoded)
          return {
              false,
              {},
              common_failure("GrowthTradeGrantSectionMismatch", section.domain)
                  .error};
        r = visit_growth_trade_grants_v1(reader, trade_grants);
        trade_grants_decoded = true;
      } else if (section.domain == "growth_terminal_publications") {
        if (!terminal_selected || section.codec_version != 1 ||
            terminal_decoded)
          return {false,
                  {},
                  common_failure("GrowthTerminalPublicationSectionMismatch",
                                 section.domain)
                      .error};
        r = visit_growth_terminal_publications_v1(reader,
                                                  terminal_publications);
        terminal_decoded = true;
      }
#include "resident/domains/local_economy/world_codec/checkpoint_read_household_purchases.inc"
#include "resident/domains/commerce/world_codec/checkpoint_read_household_retail_settlements.inc"
#include "resident/facility_claims/world_codec/checkpoint_read_facility_claims.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_production_cycles.inc"
      else if (section.domain == "civilian_transport_population") {
        auto population_version =
            selected_native_population_transport_version(catalog.identity);
        if (!population_version.ok)
          return {false, {}, std::move(population_version.error)};
        if (!population_version.value ||
            section.codec_version !=
                static_cast<std::uint32_t>(population_version.value) ||
            seed.population_transport)
          return {false,
                  {},
                  common_failure("PopulationTransportCheckpointProfileMismatch")
                      .error};
        auto population = make_state<Seed, PopulationTransportState>();
        r = visit_population_transport(reader, *population,
                                       bool(catalog.household_purchases),
                                       population_version.value);
        seed.population_transport = std::move(population);
      }
#include "resident/domains/commerce/world_codec/checkpoint_read_private_transport_funding.inc"
#include "resident/domains/craft/world_codec/checkpoint_read_item_instances.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_economic_facility_capacity_sources.inc"
      else if (section.domain == "public_supply_inspection") {
        auto selected =
            selected_native_public_supply_inspection(catalog.identity);
        if (!selected.ok)
          return {false, {}, std::move(selected.error)};
        if (selected.value != 1 || section.codec_version != 1 ||
            !catalog.public_supply_inspection || public_supply_inspection)
          return {false,
                  {},
                  common_failure(
                      "PublicSupplyInspectionCheckpointSelectionMismatch")
                      .error};
        public_supply_inspection =
            make_state<Seed, PublicSupplyInspectionState>();
        r = visit_public_supply_inspection_checkpoint(
            reader, *public_supply_inspection);
      } else if (section.domain == "field_battle") {
        auto selected = selected_field_battle_version(catalog.identity);
        if (!selected.ok)
          return {false, {}, selected.error};
        if (selected.value != 1 ||
            section.codec_version != field_battle_checkpoint_codec_revision ||
            !catalog.field_battle || field_battle)
          return {
              false,
              {},
              common_failure("FieldBattleCheckpointSelectionMismatch").error};
        field_battle = make_state<Seed, FieldBattleWorldState>();
        r = visit_field_battle_world(reader, *field_battle);
      }
#include "resident/domains/military/world_codec/checkpoint_read_dynamic_military.inc"
#include "resident/domains/commerce/world_codec/checkpoint_read_private_shop_operation_sources.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_private_shop_custody.inc"
#include "resident/domains/family/world_codec/checkpoint_read_control_continuation.inc"
#include "resident/checkpoint/world_codec/checkpoint_read_profile_deliveries.inc"
#include "resident/domains/actor_foundation/world_codec/checkpoint_read_actor_start_context.inc"
#include "resident/domains/growth/world_codec/checkpoint_read_growth_permanent_contributions.inc"
#include "resident/domains/actor_foundation/world_codec/checkpoint_read_character_condition_extensions.inc"
      else if (section.domain == "political_passage") {
        if (!catalog.field_group_passage || political_passage || political_safe_exit || section.codec_version != 3)
          return {false, {}, common_failure("FieldPassageCheckpointSelectionMismatch").error};
        political_passage = make_state<Seed, PoliticalPassageDecisionState>();
        political_safe_exit = make_state<Seed, SafeExitState>();
        r = visit_field_passage_checkpoint(reader, *political_passage, *political_safe_exit);
      }
      else if (section.domain == "political_relations") {
        if (!catalog.political_relations || political_relations ||
            section.codec_version != 1)
          return {
              false,
              {},
              common_failure("PoliticalRelationsCheckpointSelectionMismatch")
                  .error};
        political_relations = make_state<Seed, PoliticalRelationsState>();
        r = visit_political_relations(reader, *political_relations);
      } else if (section.domain == "character-injury-sources") {
        if (injury_version.value != 1 || section.codec_version != 1 ||
            injury_sources)
          return {false,
                  {},
                  common_failure("CharacterInjuryCheckpointSelectionMismatch")
                      .error};
        injury_sources = make_state<Seed, CharacterInjurySourcesStateV1>();
        r = visit_character_injury_sources_v1(reader, *injury_sources);
      }

#include "resident/domains/local_economy/world_codec/checkpoint_read_local_economy.inc"
#include "resident/domains/military/world_codec/checkpoint_read_population_military.inc"
#include "resident/domains/organization/world_codec/checkpoint_read_organization.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_economic_obligation_sources.inc"
#include "resident/domains/affairs/world_codec/checkpoint_read_affairs.inc"
#include "resident/domains/director/world_codec/checkpoint_read_director.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_treaty_passage.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_occupation_fiscal_settlement.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_political_fiscal.inc"
#include "resident/domains/military/world_codec/checkpoint_read_military_officer_political_module.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_military_field_command_political_module.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_economic_takeover_execution.inc"
#include "resident/domains/tasks/world_codec/checkpoint_read_task_takeover_execution.inc"
#include "resident/domains/interaction/world_codec/checkpoint_read_personal_appointments.inc"
#include "resident/domains/growth/world_codec/checkpoint_read_growth_tactic_lessons.inc"
#include "resident/domains/interaction/world_codec/checkpoint_read_known_personal_dialogues.inc"
#include "resident/domains/tasks/world_codec/checkpoint_read_task_shop_order_contact_module.inc"
#include "resident/domains/family/world_codec/checkpoint_read_family_collateral_legacy.inc"
#include "resident/domains/tasks/world_codec/checkpoint_read_task_message_return_module.inc"
#include "resident/domains/family/world_codec/checkpoint_read_family_biological_legacy.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_political_succession_domain.inc"
#include "resident/domains/interaction/world_codec/checkpoint_read_visit_message_receipt_module.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_political_office_reception_domain.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_household_provision_management.inc"
#include "resident/domains/family/world_codec/checkpoint_read_family_life.inc"
#include "resident/domains/growth/world_codec/checkpoint_read_personal_injury_recovery.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_political_shop_licensing_receipts.inc"
#include "resident/domains/commerce/world_codec/checkpoint_read_private_shop_liquidations.inc"
#include "resident/domains/tasks/world_codec/checkpoint_read_task_shop_order_estate_cancellations.inc"
#include "resident/domains/family/world_codec/checkpoint_read_family_apprenticeship.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_political_governance_continuations.inc"
#include "resident/domains/family/world_codec/checkpoint_read_family_disciple_legacy.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_governance_invitation_reply_module.inc"
#include "resident/domains/commerce/world_codec/checkpoint_read_private_shop_daily_rent_module.inc"
#include "resident/domains/interaction/world_codec/checkpoint_read_visit_messages.inc"
#include "resident/domains/estate/world_codec/checkpoint_read_estate_employment.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_economic_supply_policies.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_political_supply_adoptions.inc"
#include "resident/domains/local_economy/world_codec/checkpoint_read_fiscal_taxpayer_continuity.inc"
#include "resident/domains/player_plan/world_codec/checkpoint_read_player_plan.inc"
#include "resident/domains/tasks/world_codec/checkpoint_read_governance_tasks.inc"
#include "resident/domains/social/world_codec/checkpoint_read_dynamic_social.inc"
#include "resident/domains/interaction/world_codec/checkpoint_read_face_to_face.inc"
#include "resident/domains/interaction/world_codec/checkpoint_read_actor_social_occupancy.inc"
#include "resident/domains/growth/world_codec/checkpoint_read_growth_market_history.inc"
#include "resident/domains/politics/world_codec/checkpoint_read_political_shop_licensing.inc"
#include "resident/domains/commerce/world_codec/checkpoint_read_private_shops.inc"
#include "resident/domains/tasks/world_codec/checkpoint_read_task_shop_orders.inc"
#include "resident/domains/military/world_codec/checkpoint_read_military_tactic_learning_module.inc"
      else
        r = common_failure("UnknownCheckpointDomain", section.domain);
      if (!r.ok)
        return {false, {}, r.error};
      r = reader.finish();
      if (!r.ok)
        return {false, {}, r.error};
      if (section.domain == "growth")
        growth_body_decoded = true;
      if (transport_context &&
          ((!transport_versions.value.manifest && section.domain == "travel") ||
           section.domain == "transport_manifest_bindings")) {
        r = finish_transport_physical_codec(*transport_context);
        if (!r.ok)
          return {false, {}, std::move(r.error)};
        transport_context.reset();
        seed.common = std::move(transport_common->state());
        transport_common.reset();
        transport_finished = true;
      }
    }
    if (trade_grants_selected != trade_grants_decoded ||
        (trade_grants_selected && !growth_body_decoded) ||
        !growth->trade_grants.empty())
      return {false,
              {},
              common_failure("GrowthTradeGrantSectionMismatch", "trade-grant")
                  .error};
    if (trade_grants_decoded)
      growth->trade_grants = std::move(trade_grants);
    if (terminal_selected != terminal_decoded ||
        (terminal_selected && !growth_body_decoded) ||
        !growth->terminal_publications.empty())
      return {false,
              {},
              common_failure("GrowthTerminalPublicationSectionMismatch",
                             "growth_terminal_publications")
                  .error};
    if (terminal_decoded)
      growth->terminal_publications = std::move(terminal_publications);
    if (bool(boundary_version.value) != boundary_decoded)
      return {
          false,
          {},
          common_failure("MovementBoundaryCheckpointSectionMismatch").error};
    if (movement) {
      auto headers = validate_native_movement_boundary_headers(
          seed.common, catalog.identity, *movement);
      if (!headers.ok)
        return {false, {}, std::move(headers.error)};
    }
    if (bool(continuation_version.value) != continuation_decoded)
      return {false, {}, common_failure("MovementBoundaryContinuationCheckpointSectionMismatch").error};
    auto continuation_headers = validate_native_movement_boundary_continuation_headers(
        seed.common, catalog.identity, movement.get());
    if (!continuation_headers.ok)
      return {false, {}, std::move(continuation_headers.error)};
    if (bool(local_move_version.value) != local_move_decoded)
      return {false, {}, common_failure("LocalMoveCheckpointSectionMismatch").error};
    auto local_move_headers = validate_native_local_move_headers(
        seed.common, catalog.identity, movement.get());
    if (!local_move_headers.ok)
      return {false, {}, std::move(local_move_headers.error)};
    if (bool(handoff_versions.value.releases) != member_releases_decoded ||
        bool(handoff_versions.value.losses) != member_losses_decoded)
      return {
          false,
          {},
          common_failure("GroupMemberHandoffCheckpointSectionMismatch").error};
    auto handoff_headers = validate_native_group_member_handoff_headers(
        seed.common, catalog.identity, executions.get());
    if (!handoff_headers.ok)
      return {false, {}, std::move(handoff_headers.error)};
    if (transport_versions.value.travel) {
      if (!transport_finished || transport_context || transport_common)
        return {false,
                {},
                common_failure("IncompleteTransportPhysicalRestore").error};
      seed.movement_exact = std::move(movement);
      seed.spatial_carriers = std::move(carriers);
      seed.group_movement_executions = std::move(executions);
      seed.transport_registrations = std::move(registrations);
      seed.economic_transport_expenses = std::move(payments);
    }
    if (catalog.growth->complete_capability_application_chain !=
            bool(seed.growth_permanent) ||
        catalog.growth->complete_capability_application_chain !=
            bool(seed.card_acquisitions))
      return {false,
              {},
              common_failure("GrowthPermanentSelectedStatePairRequired").error};
    {
      CommonStore view(seed.common, catalog.common);
      auto exact = validate_native_task_execution_layout(view, catalog, *tasks);
      if (!exact.ok)
        return {false, {}, std::move(exact.error)};
    }
    auto task_work_layout = validate_native_task_work_layout(catalog, *tasks);
    if (!task_work_layout.ok)
      return {false, {}, std::move(task_work_layout.error)};
    {
      CommonStore view(seed.common, catalog.common);
      const auto *work =
          tasks->work_execution ? &*tasks->work_execution : nullptr;
      auto headers = validate_task_work_original_fact_headers(view, work);
      if (!headers.ok)
        return {false, {}, std::move(headers.error)};
      headers = validate_task_work_original_action_headers(view, work);
      if (!headers.ok)
        return {false, {}, std::move(headers.error)};
    }
    auto item_craft_versions =
        selected_native_item_craft_versions(catalog.identity);
    if (!item_craft_versions.ok)
      return {false, {}, std::move(item_craft_versions.error)};
    if (bool(item_craft_versions.value.item_instances) !=
            bool(seed.item_instances) ||
        bool(item_craft_versions.value.lot_properties) !=
            bool(seed.lot_properties) ||
        bool(item_craft_versions.value.craft_jobs) != bool(seed.craft_jobs))
      return {false, {}, common_failure("ItemCraftSelectedRootMismatch").error};
    {
      CommonStore original(seed.common, catalog.common);
      auto actions = validate_native_item_craft_original_actions(
          original, catalog, seed.item_instances.get(), seed.craft_jobs.get());
      if (!actions.ok)
        return {false, {}, std::move(actions.error)};
      auto headers = validate_native_item_craft_original_headers(
          original, catalog, seed.item_instances.get(), seed.craft_jobs.get());
      if (!headers.ok)
        return {false, {}, std::move(headers.error)};
    }
    auto facility_versions =
        selected_native_economic_facility_versions(catalog.identity);
    if (!facility_versions.ok)
      return {false, {}, std::move(facility_versions.error)};
    if (bool(facility_versions.value.facilities) !=
            bool(seed.economic_facility_capacity_sources) ||
        bool(facility_versions.value.conditions) !=
            bool(seed.production_facility_conditions) ||
        bool(facility_versions.value.work_expense) !=
            bool(seed.governance_work_expense))
      return {false,
              {},
              common_failure("EconomicFacilitySelectedRootMismatch").error};
    {
      CommonStore original(seed.common, catalog.common);
      auto headers = validate_native_economic_facility_headers(
          original, catalog, economy.get(),
          seed.economic_facility_capacity_sources.get(),
          seed.production_facility_conditions.get(),
          seed.governance_work_expense.get());
      if (!headers.ok)
        return {false, {}, std::move(headers.error)};
    }
    auto population_version =
        selected_native_population_transport_version(catalog.identity);
    if (!population_version.ok)
      return {false, {}, std::move(population_version.error)};
    if (bool(population_version.value) != bool(seed.population_transport) ||
        (seed.population_transport &&
         seed.population_transport->version != population_version.value))
      return {false,
              {},
              common_failure("PopulationTransportSelectedStateMismatch").error};
    if (bool(catalog.production_cycles) != bool(seed.production_cycles))
      return {false,
              {},
              common_failure("ProductionCyclesSelectedStatePairRequired", {},
                             ErrorKind::internal)
                  .error};
    if (catalog.private_transport_funding_selected !=
        bool(seed.private_transport_funding))
      return {false,
              {},
              common_failure("PrivateTransportFundingSelectedStatePairRequired")
                  .error};
    if (bool(catalog.control_continuation) != bool(seed.control_continuation))
      return {
          false,
          {},
          common_failure("ControlContinuationSelectedStatePairRequired").error};
    auto military_versions =
        selected_native_military_state_versions(catalog.identity);
    if (!military_versions.ok)
      return {false, {}, std::move(military_versions.error)};
    if (bool(military_versions.value.dynamic_military) !=
            bool(seed.military_exact) ||
        bool(military_versions.value.military_action_time) !=
            bool(military_action_time))
      return {
          false, {}, common_failure("MilitarySelectedStatePairRequired").error};
    auto shop_versions =
        selected_native_private_shop_operation_versions(catalog.identity);
    if (!shop_versions.ok)
      return {false, {}, std::move(shop_versions.error)};
    if (bool(shop_versions.value.sources) !=
            bool(seed.private_shop_operation_sources) ||
        bool(shop_versions.value.capabilities) !=
            bool(seed.private_shop_operation_capabilities))
      return {
          false, {}, common_failure("ShopOperationSelectedRootMismatch").error};
    if (bool(catalog.private_shop_household_custody) !=
        bool(seed.private_shop_household_custody))
      return {false,
              {},
              common_failure("PrivateShopCustodySelectedRootMismatch").error};
    if (bool(catalog.life_age_checks) != bool(seed.life_age_checks))
      return {
          false, {}, common_failure("LifeAgeSelectedStatePairRequired").error};
    auto face_layout = validate_native_face_to_face_layout(catalog, *i);
    if (!face_layout.ok)
      return {false, {}, std::move(face_layout.error)};
    if (catalog.actor_social_occupancy_selected != bool(i->social_occupancy))
      return {false,
              {},
              common_failure("ActorSocialOccupancyLayoutMismatch").error};
    if ((catalog.exact_time && exact_complete_selected(*catalog.exact_time)) !=
        bool(seed.facility_claims))
      return {false, {}, common_failure("FacilityClaimsLayoutMismatch").error};
    auto delivery_version = profile_delivery_selected_version(catalog.identity);
    if (!delivery_version.ok)
      return {false, {}, std::move(delivery_version.error)};
    if ((delivery_version.value == 1) != bool(seed.profile_deliveries))
      return {false, {}, common_failure("ProfileDeliveryLayoutMismatch").error};
    if (bool(catalog.family_voluntary_paths) !=
        bool(seed.family_voluntary_paths))
      return {false,
              {},
              common_failure("FamilyVoluntaryPathProfileMismatch").error};
    if (bool(catalog.naval) != bool(seed.naval))
      return {false, {}, common_failure("NavalProfileMismatch").error};
    if (catalog.governance_tasks_enabled != bool(seed.politics_governance))
      return {false,
              {},
              common_failure(catalog.governance_tasks_enabled
                                 ? "MissingPoliticsGovernanceState"
                                 : "UnexpectedPoliticsGovernanceState")
                  .error};
    const auto &identity = index.identity;
    if (seed.common.identity.world_id != identity.world_id ||
        seed.common.identity.tick != identity.tick ||
        seed.common.identity.revision != identity.revision)
      return {false, {}, common_failure("CheckpointIdentityMismatch").error};
    if (catalog.governance_tasks_enabled != bool(seed.governance_tasks))
      return {false,
              {},
              common_failure(catalog.governance_tasks_enabled
                                 ? "MissingGovernanceTasksState"
                                 : "UnexpectedGovernanceTasksState")
                  .error};
    auto diplomatic_layout = validate_diplomacy_layout(
        catalog.envoy_tasks_enabled, catalog.identity.module_versions,
        bool(seed.diplomacy), bool(seed.envoy_tasks),
        catalog.diplomacy ? catalog.diplomacy->version : 1);
    if (!diplomatic_layout.ok)
      return {false, {}, diplomatic_layout.error};
    auto responses_valid = validate_task_offer_response_version_layout(
        catalog.task_offer_response_version, catalog.identity.module_versions,
        tasks->offer_responses ? &*tasks->offer_responses : nullptr);
    if (!responses_valid.ok)
      return {false, {}, responses_valid.error};
    if (catalog.envoy_tasks_enabled != bool(seed.envoy_tasks))
      return {false,
              {},
              common_failure(catalog.envoy_tasks_enabled
                                 ? "MissingEnvoyTasksState"
                                 : "UnexpectedEnvoyTasksState")
                  .error};
    if (catalog.exact_time) {
      seed.world_timeline = std::move(exact_timeline);
      seed.action_elapsed = std::move(exact_elapsed);
      seed.actor_foundation = std::move(actor_foundation);
    }
    {
      CommonStore original(seed.common, catalog.common);
      auto checked = validate_native_condition_application_headers(
          original, catalog, seed.actor_foundation.get());
      if (!checked.ok)
        return {false, {}, std::move(checked.error)};
    }
    if (military_action_time) {
      auto selected = selected_native_military_state_versions(catalog.identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      auto conditions = validate_military_action_condition_layout(
          *military_action_time, selected.value.military_action_conditions);
      if (!conditions.ok)
        return {false, {}, std::move(conditions.error)};
    }
    {
      CommonStore original(seed.common, catalog.common);
      auto training = validate_native_military_training_completion_headers(
          original, *m, military_action_time.get(),
          military_versions.value.military_training_completion_time);
      if (!training.ok)
        return {false, {}, std::move(training.error)};
    }
    if (military_action_time) {
      CommonStore original(seed.common, catalog.common);
      auto reservations = validate_military_formation_reservation_sources(
          original, *catalog.military, *military_action_time,
          military_versions.value.military_formation_reservations);
      if (!reservations.ok)
        return {false, {}, std::move(reservations.error)};
    }
    {
      auto effects =
          selected_native_organization_service_effects(catalog.identity);
      if (!effects.ok)
        return {false, {}, std::move(effects.error)};
      if ((effects.value == 1) != organization_effects_decoded)
        return {
            false,
            {},
            common_failure("OrganizationServiceEffectsSelectedLayoutMismatch")
                .error};
      auto layout =
          validate_organization_service_effects(*organization, effects.value);
      if (!layout.ok)
        return {false, {}, std::move(layout.error)};
    }
    {
      auto selected =
          selected_native_personal_battle_lifecycle(catalog.identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      if (!personal_body_decoded ||
          bool(selected.value) != personal_lifecycle_decoded)
        return {false,
                {},
                common_failure("PersonalBattleLifecycleSelectedSectionMismatch")
                    .error};
      auto layout =
          validate_personal_battle_lifecycle_layout(*personal, selected.value);
      if (!layout.ok)
        return {false, {}, std::move(layout.error)};
    }
    {
      auto selected =
          selected_native_military_battle_lifecycle(catalog.identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      if (!military_body_decoded ||
          bool(selected.value) != military_lifecycle_decoded)
        return {false,
                {},
                common_failure("MilitaryBattleLifecycleSelectedSectionMismatch")
                    .error};
      auto layout =
          validate_military_battle_lifecycle_layout(*m, selected.value);
      if (!layout.ok)
        return {false, {}, std::move(layout.error)};
    }
    auto field_layout = validate_field_battle_layout(
        catalog.identity, catalog.field_battle.get(), field_battle.get());
    if (!field_layout.ok)
      return {false, {}, field_layout.error};
    {
      CommonStore original(seed.common, catalog.common);
      auto checked = validate_field_battle_fact_headers(original, field_battle.get());
      if (!checked.ok)
        return {false, {}, checked.error};
      // 物理段先核所有原头反覆盖；真实候选构造后由原完整 World 门验证来源。
      if (field_battle &&
          !(catalog.exact_time && exact_complete_selected(*catalog.exact_time))) {
        checked = validate_field_battle_world(
            original, *m, *catalog.travel, *catalog.field_battle, *field_battle);
        if (!checked.ok)
          return {false, {}, checked.error};
      }
    }
    {
      CommonStore original(seed.common, catalog.common);
      auto inspection = validate_native_public_supply_inspection_layout(
          catalog, original, public_supply_inspection.get());
      if (!inspection.ok)
        return {false, {}, std::move(inspection.error)};
    }
    auto injury_layout = validate_character_injury_sources_v1_layout(
        catalog.identity, injury_sources.get());
    if (!injury_layout.ok)
      return {false, {}, std::move(injury_layout.error)};
    seed.character_injury_sources = std::move(injury_sources);
    if (bool(catalog.political_relations) != bool(political_relations))
      return {
          false,
          {},
          common_failure("PoliticalRelationsCheckpointStateRequired").error};
    if (political_relations) {
      auto shape = validate_political_relations_shape(*political_relations);
      if (!shape.ok)
        return {false, {}, std::move(shape.error)};
    }
    seed.political_relations = std::move(political_relations);
    if (bool(catalog.field_group_passage) != bool(political_passage) ||
        bool(catalog.field_group_passage) != bool(political_safe_exit))
      return {false, {}, common_failure("FieldPassageCheckpointPresenceMismatch").error};
    seed.political_passage = std::move(political_passage);
    seed.political_safe_exit = std::move(political_safe_exit);
    seed.public_supply_inspection = std::move(public_supply_inspection);
    seed.field_battle = std::move(field_battle);
    seed.military_action_time = std::move(military_action_time);
    seed.travel = std::move(t);
    seed.military = std::move(m);
    seed.interaction = std::move(i);
    seed.social = std::move(s);
    seed.estate = std::move(e);
    seed.tasks = std::move(tasks);
    seed.family = std::move(family);
    seed.personal = std::move(personal);
    seed.growth = std::move(growth);
    if (catalog.dynamic_profile == DynamicProfile::local_society_v1) {
      seed.local_economy = std::move(economy);
      seed.organization = std::move(organization);
      seed.affairs = std::move(affairs);
      seed.director = std::move(director);
    }
    if (population_enabled(catalog))
      seed.population_military = std::move(population);
    if (catalog.player_plans)
      seed.player_plan = std::move(plan);
    if (politics_enabled(catalog))
      seed.politics = std::move(politics);
    if (political_fiscal_enabled(catalog))
      seed.political_fiscal = std::move(fiscal);
    if (settlement_enabled(catalog))
      seed.fiscal_settlement = std::move(settlement);
    if (household_management_enabled(catalog))
      seed.household_provision_management = std::move(household_management);
    if (disruption_enabled(catalog))
      seed.production_disruptions = std::move(disruption);
    if (relief_enabled(catalog))
      seed.public_relief = std::move(relief);
    if (family_life_enabled(catalog))
      seed.family_life = std::move(life);
    if (family_legacy_enabled(catalog))
      seed.family_legacy = std::move(legacy);
    if (family_bequest_enabled(catalog))
      seed.family_bequest = std::move(bequest);
    if (family_inheritance_enabled(catalog))
      seed.family_inheritance = std::move(inheritance);
    if (family_marriage_enabled(catalog))
      seed.family_marriage = std::move(marriage);
    if (family_birth_enabled(catalog))
      seed.family_birth = std::move(birth);
    if (family_care_relief_enabled(catalog))
      seed.child_care_relief = std::move(care_relief);
    if (catalog.personal_recovery)
      seed.personal_recovery = std::move(recovery);
    if constexpr (std::is_same_v<Seed, DecodedWorldSeedOwner>)
      seed.decoded_ = true;
    return {true, std::move(seed), {}};
  }
};
