#include "native_group_member_handoffs.hpp"
#include "native_political_relations.hpp"
#include "native_field_passage.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1_state.hpp"
#include "native_character_injury_state.hpp"
#include "exact_time_profile.hpp"
#include "native_actor_social_occupancy.hpp"
#include "native_born_population_state.hpp"
#include "native_catalog_decode.hpp"
#include "native_catalog_identity.hpp"
#include "native_control_continuation.hpp"
#include "native_director_catalog.hpp"
#include "native_economic_facility_roots.hpp"
#include "native_economic_obligation_sources.hpp"
#include "native_face_to_face_sources.hpp"
#include "native_family_birth_state.hpp"
#include "native_governance_programs.hpp"
#include "native_growth_permanent_catalog.hpp"
#include "native_household_catalog.hpp"
#include "native_interaction_catalog.hpp"
#include "native_item_craft_state.hpp"
#include "native_life_age_catalog.hpp"
#include "native_life_age_world.hpp"
#include "native_military_battle_lifecycle.hpp"
#include "native_military_occupation.hpp"
#include "native_military_state.hpp"
#include "native_movement_boundary_catalog.hpp"
#include "native_movement_boundary_continuation_catalog.hpp"
#include "native_local_move_catalog.hpp"
#include "native_organization_runtime.hpp"
#include "native_personal_battle_lifecycle.hpp"
#include "native_private_shop_household_custody.hpp"
#include "native_private_shop_operations.hpp"
#include "native_production_cycles.hpp"
#include "native_public_supply_inspection.hpp"
#include "native_social_selection.hpp"
#include "native_task_execution_state.hpp"
#include "native_task_work_catalog.hpp"
#include "native_transport_catalog.hpp"
#include "native_transport_funding.hpp"
#include "original_calendar.hpp"
#include "original_catalog_identity.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/validated_catalog.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "resident/domains/commerce/command_results/private_shop_command_results_codec.hpp"
#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.hpp"
#include "resident/domains/family/biological_control_world.hpp"
#include "resident/domains/family/care_root_capability_selection.hpp"
#include "resident/domains/family/collateral_control_world.hpp"
#include "resident/domains/family/disciple_control_receipts.hpp"
#include "resident/domains/family/family_apprenticeship.hpp"
#include "resident/domains/family/innate_study_profile.hpp"
#include "resident/domains/family/voluntary_path_state.hpp"
#include "resident/domains/growth/growth_capability_chain.hpp"
#include "resident/domains/growth/growth_market_initial.hpp"
#include "resident/domains/growth/growth_tactic_lesson_boundary.hpp"
#include "resident/domains/growth/growth_tactic_lesson_codec.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_attendance_world.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/party_consent/party_recruitment_consent.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_intents.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/local_economy/disruptions/production_disruptions_codec.hpp"
#include "resident/domains/local_economy/economic_facility_provider.hpp"
#include "resident/domains/local_economy/household/provision_management_codec.hpp"
#include "resident/domains/local_economy/market_service_original.hpp"
#include "resident/domains/local_economy/private_shop_household_custody_codec.hpp"
#include "resident/domains/local_economy/private_shop_household_custody_fields.hpp"
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/military/military_combat_conditions_profile.hpp"
#include "resident/domains/military/military_command_resources_profile.hpp"
#include "resident/domains/military/military_command_shock_profile.hpp"
#include "resident/domains/military/military_field_command_profile.hpp"
#include "resident/domains/military/military_field_command_rules.hpp"
#include "resident/domains/military/military_field_terrain_profile.hpp"
#include "resident/domains/military/military_garrison_rest_profile.hpp"
#include "resident/domains/military/military_officer_codec.hpp"
#include "resident/domains/military/military_officer_profile.hpp"
#include "resident/domains/military/military_officer_rules.hpp"
#include "resident/domains/military/military_recovery_profile.hpp"
#include "resident/domains/military/military_rout_profile.hpp"
#include "resident/domains/military/military_sequential_profile.hpp"
#include "resident/domains/military/military_siege_engineering_profile.hpp"
#include "resident/domains/military/military_supply_attrition_profile.hpp"
#include "resident/domains/military/military_tactic_learning_profile.hpp"
#include "resident/domains/military/military_tactic_learning_source.hpp"
#include "resident/domains/organization/organization_authored_fixed_origin.hpp"
#include "resident/domains/organization/organization_authored_initialization.hpp"
#include "resident/domains/organization/organization_authored_origin.hpp"
#include "resident/domains/organization/organization_lifecycle_catalog.hpp"
#include "resident/domains/personal/personal_simultaneous_outcomes.hpp"
#include "resident/domains/player_plan/plan_profile.hpp"
#include "resident/domains/politics/governance_agency_intents/political_governance_agency_intents.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_author_profile.hpp"
#include "resident/domains/politics/political_author_selection.hpp"
#include "resident/domains/social/social_period_sources.hpp"
#include "resident/domains/tasks/extensions/task_extension_world.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff_profile.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset.hpp"
#include "resident/domains/tasks/task_authored_continuation_catalog.hpp"
#include <new>
#include <optional>
#include <stdexcept>

// 目录直建新世界；投影只存在于冷边界，旧域与宏观域初值最终合成同一typed seed。
#include "catalog_copy_plan.hpp"
#include "economic_supply_policy_integration.hpp"
#include "estate_employment_integration.hpp"
#include "native_actor_selection.hpp"
#include "native_appearance_selection.hpp"
#include "native_initial_state.hpp"
#include "native_player_control.hpp"
#include "native_skill_profile.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation_boundary.hpp"
#include "resident/domains/director/director_model.hpp"
#include "resident/domains/family/child_care_relief.hpp"
#include "resident/domains/family/family_bequest.hpp"
#include "resident/domains/family/family_birth.hpp"
#include "resident/domains/family/family_death.hpp"
#include "resident/domains/family/family_inheritance.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/family_marriage.hpp"
#include "resident/domains/interaction/party_invitation_profile.hpp"
#include "resident/domains/interaction/regional_life_dialogue.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_need_profile.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/field_battle/field_world.hpp"
#include "resident/domains/military/naval_codec.hpp"
#include "resident/domains/military/naval_combat_authoring.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_profile.hpp"
#include "resident/domains/politics/governance_continuation/political_governance_continuation.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_result_bindings.hpp"
#include "resident/domains/politics/licensing/political_shop_registry_model.hpp"
#include "resident/domains/politics/politics_fiscal.hpp"
#include "resident/domains/politics/politics_governance_profile.hpp"
#include "resident/domains/tasks/task_offer_response_profile.hpp"
#include "resident/domains/tasks/task_personal_advance_profile.hpp"
#include "resident/model/common_rng.hpp"
#include "world_codec.hpp"
#include <picosha2.h>
#include <set>
namespace hanmo::resident {
// 原 native 冷准入把已核真实资源的拥有者交给同一目录构造，不新增公共头接口。
Outcome<std::shared_ptr<const PreparedCatalog>>
prepare_catalog_with_original_field(
    const BoundaryValue &, const BoundaryValue &,
    std::shared_ptr<const FieldBattleCatalog>);
// 与ResidentWorld原安装共用唯一分配器；不改共享头或冷恢复身份。
WorldId allocate_native_world_id();
Status seed_native_actors(CommonState &, const BoundaryValue &,
                          const BoundaryValue &, TravelState &,
                          InteractionState &, bool,
                          const ActorFoundationCatalog *);
namespace {
using namespace boundary;
using V = BoundaryValue;
// Only the unpublished native initializer uses an integral clock without a
// WorldBundle. It reads the actual candidate tick on every call and grants no
// stamp or revision.
class NativeInitialMomentRead final : public CurrentMomentRead,
                                      public OrganizationAuthorityClock {
  const CommonStore &common_;

public:
  explicit NativeInitialMomentRead(const CommonStore &common)
      : common_(common) {}
  Outcome<WorldMoment> read_current_moment() const override {
    if (common_.tick() != 0 || common_.revision() != 0)
      return {false, {}, common_failure("NativeInitialMomentRequired").error};
    return {true, WorldMoment{common_.tick(), {}}, {}};
  }
  // 同一个 fresh 候选原钟，不另用请求时间或保存默认零时刻。
  Outcome<WorldMoment> read_organization_authority_now() const override {
    return read_current_moment();
  }
};
// 只在第二阶段同步借用真实起点源；terminal 仍走旧原 reader 的同一正文。
class NativeInitialOrganizationOriginRead final
    : public OrganizationAuthoredOriginSourceRead {
  const NativeOrganizationAuthoredOriginRead &terminal_;
  const NativeOrganizationAuthoredFixedOriginRead &fixed_;

public:
  NativeInitialOrganizationOriginRead(
      const NativeOrganizationAuthoredOriginRead &terminal,
      const NativeOrganizationAuthoredFixedOriginRead &fixed)
      : terminal_(terminal), fixed_(fixed) {}
  Status
  validate_authored_origin(const OrganizationRecord &body,
                           const LifecycleSeedPolicy &policy,
                           const CatalogIdentity &identity) const override {
    if (policy.initial_lifecycle == OrganizationStatus::terminal)
      return terminal_.validate_authored_origin(body, policy, identity);
    return fixed_.validate_authored_origin(body, policy, identity);
  }
};
// 冷投影只覆盖显式初态字段；未知表/记录由作者准入与各域typed decoder继续拒绝。
template <class T> void reserve_projection_append(std::vector<T> &rows) {
  static_assert(std::is_nothrow_move_constructible_v<T>,
                "original boundary growth moves already owned fields");
  if (rows.size() < rows.capacity())
    return;
  const auto size = rows.size(), maximum = rows.max_size();
  if (size == maximum)
    boundary::fail("CheckpointCollectionTooLarge", "native/effective_catalog");
  const auto count = size > maximum - size ? maximum : (size ? size * 2 : 1);
  checkpoint_decode_boundary_array(count, sizeof(T));
  rows.reserve(count);
}
void put(V &target, std::string_view key, V value) {
  auto &rows = *target.as<V::Dict>();
  for (auto &row : rows)
    if (row.first == key) {
      row.second = std::move(value);
      return;
    }
  reserve_projection_append(rows);
  rows.emplace_back(checkpoint_decode_boundary_text(key), std::move(value));
}
// 新剧本按显式活动集合建立初态；被引用的被动定义保持只读，不隐式激活人物。
V effective_catalog(const V &catalog, const NativeInitRequest &request) {
  auto effective = checkpoint_decode_boundary_copy(catalog);
  object(field(effective, "data", "catalog"), "catalog/data");
  auto &data = *effective.find("data");
  auto source = checkpoint_decode_boundary_copy(data);
  object(field(source, "scenarios", "data"), "data/scenarios");
  field(*source.find("scenarios"), request.scenario_id, "scenarios");
  auto *selected_scenario = source.find("scenarios")->find(request.scenario_id);
  const auto &checked_initial =
      field(*selected_scenario, "initial", request.scenario_id);
  fields(checked_initial, {"actors", "offer_ids"},
         {"wallets", "inventories", "roles", "relations", "estates",
          "garrisons", "offices", "strongholds"},
         "initial");
  auto *selected_initial = selected_scenario->find("initial");
  const auto &selected_enabled =
      field(*selected_scenario, "enabled", request.scenario_id);
  object(selected_enabled, "enabled");
  for (const auto *table :
       {"actors", "wallets", "inventories", "roles", "relations", "estates",
        "garrisons", "offices", "strongholds"}) {
    std::set<StableId, std::less<StableId>, CheckpointAllocator<StableId>>
        enabled_ids, initial_ids;
    if (const auto *values = selected_enabled.find(table))
      for (auto &id : strings(
               *values, checkpoint_decode_boundary_join({"enabled/", table})))
        if (!enabled_ids.insert(std::move(id)).second)
          fail("DuplicateScenarioEnabled", table);
    if (const auto *values = selected_initial->find(table))
      for (const auto &row : array(
               *values, checkpoint_decode_boundary_join({"initial/", table}))) {
        const auto key = std::string_view(table) == "actors"  ? "actor_id"
                         : std::string_view(table) == "roles" ? "role_id"
                                                              : "id";
        const auto &id = get_string_ref(row, key, table);
        if (!initial_ids.insert(checkpoint_decode_boundary_string(id)).second)
          fail_parts("DuplicateScenarioInitial", {table, "/", id});
        if (std::string_view(table) == "actors")
          fields(row,
                 {"actor_id", "place_id", "alive", "available", "age_years",
                  "attributes", "skills", "combat", "appearance_definition"},
                 {"initial_goods"},
                 checkpoint_decode_boundary_join({"initial/actors/", id}));
      }
    if (initial_ids != enabled_ids)
      fail_parts("MissingScenarioInitial", {"initial/", table});
  }
  const auto offered = strings(field(*selected_initial, "offer_ids", "initial"),
                               "initial/offer_ids");
  const auto enabled_offers =
      selected_enabled.find("offers")
          ? strings(*selected_enabled.find("offers"), "enabled/offers")
          : std::vector<StableId>{};
  {
    // Same original set equality/duplicate rule, borrowing the two original
    // vectors. Only actual set nodes allocate, through the original job.
    using IdSet = std::set<std::string_view, std::less<std::string_view>,
                           CheckpointAllocator<std::string_view>>;
    const IdSet offered_ids(offered.begin(), offered.end()),
        allowed_ids(enabled_offers.begin(), enabled_offers.end());
    if (offered_ids != allowed_ids || offered_ids.size() != offered.size())
      fail("InvalidScenarioOffers", "initial/offer_ids");
  }
  // 与ScenarioSnapshot一致地排序无序初态；路线、外貌候选和对白数组不在此重排。
  for (auto &table : *selected_initial->as<V::Dict>())
    if (auto *rows = table.second.as<V::Array>()) {
      if (table.first == "offer_ids") {
        validate_strings(table.second, "initial/offer_ids");
        std::sort(rows->begin(), rows->end(), [](const auto &a, const auto &b) {
          return *a.template as<StableId>() < *b.template as<StableId>();
        });
        continue;
      }
      const auto key = table.first == "actors"  ? "actor_id"
                       : table.first == "roles" ? "role_id"
                                                : "id";
      std::sort(rows->begin(), rows->end(), [&](const auto &a, const auto &b) {
        return get_string_ref(a, key, table.first) <
               get_string_ref(b, key, table.first);
      });
      for (auto &row : *rows) {
        for (const auto *goods_field : {"goods", "initial_goods"})
          if (auto *goods = row.find(goods_field)) {
            array(*goods, goods_field);
            std::sort(goods->as<V::Array>()->begin(),
                      goods->as<V::Array>()->end(),
                      [](const auto &a, const auto &b) {
                        return get_string_ref(a, "item_id", "") <
                               get_string_ref(b, "item_id", "");
                      });
          }
        if (table.first == "estates") {
          if (auto *members = row.find("resident_actor_ids")) {
            validate_strings(*members, "resident_actor_ids");
            std::sort(members->as<V::Array>()->begin(),
                      members->as<V::Array>()->end(),
                      [](const auto &a, const auto &b) {
                        return *a.template as<StableId>() <
                               *b.template as<StableId>();
                      });
          }
          if (auto *duties = row.find("initial_duties")) {
            array(*duties, "initial_duties");
            std::sort(duties->as<V::Array>()->begin(),
                      duties->as<V::Array>()->end(),
                      [](const auto &a, const auto &b) {
                        return get_string_ref(a, "actor_id", "") <
                               get_string_ref(b, "actor_id", "");
                      });
          }
        }
        if (table.first == "garrisons")
          if (auto *units = row.find("units")) {
            array(*units, "units");
            std::sort(units->as<V::Array>()->begin(),
                      units->as<V::Array>()->end(),
                      [](const auto &a, const auto &b) {
                        return get_string_ref(a, "id", "") <
                               get_string_ref(b, "id", "");
                      });
            for (auto &unit : *units->as<V::Array>())
              if (auto *equipment = unit.find("equipment")) {
                array(*equipment, "equipment");
                std::sort(equipment->as<V::Array>()->begin(),
                          equipment->as<V::Array>()->end(),
                          [](const auto &a, const auto &b) {
                            return get_string_ref(a, "item_id", "") <
                                   get_string_ref(b, "item_id", "");
                          });
              }
          }
      }
    }
  const auto &scenario = *selected_scenario;
  const auto &enabled = field(scenario, "enabled", request.scenario_id);
  const auto &initial = *selected_initial;
  const std::initializer_list<std::string_view> passive = {
      "legacy_control_consent_requests",
      "succession_consent_requests",
      "adult_adoption_requests",
      "insight_cards",
      "teaching_requests",
      "study_teaching_requests",
      "actor_cards",
      "cards",
      "items",
      "dialogues",
      "personal_skills",
      "personal_battlefields",
      "personal_objectives",
      "battlefields",
      "troop_types",
      "military_objectives",
      "siege_topologies",
      "etiquette_challenges",
      "bargain_challenges",
      "drill_challenges",
      "texts",
      "actors"};
  for (auto &table : *data.as<V::Dict>()) {
    if (table.first == "required_capabilities" ||
        std::find(passive.begin(), passive.end(), table.first) !=
            passive.end() ||
        !table.second.as<V::Dict>())
      continue;
    if (table.first == "scenarios") {
      V::Dict selected;
      checkpoint_decode_boundary_array(1, sizeof(V::Dict::value_type));
      selected.reserve(1);
      selected.emplace_back(
          checkpoint_decode_boundary_string(request.scenario_id),
          checkpoint_decode_boundary_copy(scenario));
      table.second = V{std::move(selected)};
      continue;
    }
    V::Dict selected;
    if (const auto *ids = enabled.find(table.first)) {
      auto sorted_ids = strings(
          *ids, checkpoint_decode_boundary_join({"enabled/", table.first}));
      std::sort(sorted_ids.begin(), sorted_ids.end());
      checkpoint_decode_boundary_array(sorted_ids.size(),
                                       sizeof(V::Dict::value_type));
      selected.reserve(sorted_ids.size());
      for (const auto &id : sorted_ids)
        selected.emplace_back(checkpoint_decode_boundary_string(id),
                              checkpoint_decode_boundary_copy(
                                  field(table.second, id, table.first)));
    }
    table.second = V{std::move(selected)};
  }
  V::Array active_actors;
  for (const auto &row :
       array(field(initial, "actors", "initial"), "initial/actors")) {
    const auto &id = get_string_ref(row, "actor_id", "initial/actors");
    reserve_projection_append(active_actors);
    active_actors.push_back(V{checkpoint_decode_boundary_string(id)});
    auto *actor = data.find("actors")->find(id);
    if (!actor)
      fail("UnknownScenarioActor", id);
    for (const auto &alias :
         std::initializer_list<std::pair<std::string_view, std::string_view>>{
             {"place_id", "initial_place"},
             {"age_years", "initial_age_years"},
             {"attributes", "attributes"},
             {"skills", "skills"},
             {"combat", "combat"},
             {"appearance_definition", "appearance_definition"}})
      put(*actor, alias.second,
          checkpoint_decode_boundary_copy(field(row, alias.first, id)));
    put(*actor, "initial_goods",
        row.find("initial_goods")
            ? checkpoint_decode_boundary_copy(*row.find("initial_goods"))
            : V{V::Array{}});
  }
  std::sort(active_actors.begin(), active_actors.end(),
            [](const auto &a, const auto &b) {
              return *a.template as<StableId>() < *b.template as<StableId>();
            });
  put(data, "active_static_actor_ids", V{std::move(active_actors)});
  put(data, "active_entity_ids", checkpoint_decode_boundary_copy(enabled));
  for (const auto *name : {"wallets", "inventories", "roles", "relations",
                           "estates", "garrisons", "offices", "strongholds"}) {
    const auto *rows = initial.find(name);
    if (!rows)
      continue;
    for (const auto &row :
         array(*rows, checkpoint_decode_boundary_join({"initial/", name}))) {
      const auto &id = get_string_ref(
          row, std::string_view(name) == "roles" ? "role_id" : "id", name);
      auto *table = data.find(name);
      auto *target = table ? table->find(id) : nullptr;
      if (!target)
        fail_parts("ScenarioInitialNotEnabled", {name, "/", id});
      if (std::string_view(name) == "roles")
        put(*target, "initial_actor",
            checkpoint_decode_boundary_copy(field(row, "actor_id", id)));
      else
        for (const auto &value : object(row, id))
          put(*target, value.first,
              checkpoint_decode_boundary_copy(value.second));
    }
  }
  return effective;
}
// 只核对完整目录身份，不将缺少的摘要、规则或模块版本视为通配符。
// 五条旧随机流保原SHA256取种及固定引擎PCG初态；宏观命名随机由其领域另行管理。
void seed_rng(CommonState &common, const StableId &seed) {
  for (const auto *stream : {"world", "event", "duel", "battle", "minigame"}) {
    std::vector<unsigned char> digest(32);
    const auto input = seed + ":" + stream;
    picosha2::hash256(input.begin(), input.end(), digest.begin(), digest.end());
    std::uint64_t value = digest.front() & 0x7f;
    for (std::size_t i = 1; i < 8; ++i)
      value = (value << 8) | digest[i];
    const auto signed_seed = static_cast<Integer>(value);
    common.rng.insert(stream,
                      {signed_seed, godot_pcg32_seed_state(signed_seed), {}});
  }
}
// 驻军只建立作者明确名册；无需先启动GD军事初始化或创建军团。
std::shared_ptr<const MilitaryState>
seed_military(const MilitaryCatalog &catalog, Integer tick) {
  auto state = checkpoint_make_shared<MilitaryState>();
  for (const auto &definition : catalog.offices) {
    OfficeRecord office;
    office.id = definition.id;
    state->offices.push_back(std::move(office));
  }
  for (const auto &definition : catalog.garrisons) {
    GarrisonRecord camp;
    camp.id = definition.id;
    camp.last_supply_tick = tick;
    for (const auto &entry : definition.units) {
      MilitaryUnitRecord unit;
      unit.id = entry.id;
      unit.source_garrison_id = camp.id;
      unit.garrison_id = camp.id;
      unit.organization_id = definition.organization_id;
      unit.commander_actor_id = entry.commander_actor_id;
      unit.troop_type_id = entry.troop_type_id;
      for (Integer n = 1; n <= entry.initial_soldiers; ++n)
        unit.member_ids.push_back("soldier:" + entry.id + ":" +
                                  std::to_string(n));
      unit.fit = unit.member_ids;
      if (entry.initial_training || entry.initial_fatigue) {
        if (!entry.initial_training || !entry.initial_fatigue)
          fail("TrainingProfileRequired", entry.id);
        unit.training = entry.initial_training;
        unit.fatigue = entry.initial_fatigue;
        unit.training_action_id = StableId{};
      }
      camp.unit_ids.push_back(unit.id);
      state->military_units.push_back(std::move(unit));
    }
    state->garrisons.push_back(std::move(camp));
  }
  for (const auto &definition : catalog.strongholds) {
    auto camp = std::find_if(
        catalog.garrisons.begin(), catalog.garrisons.end(),
        [&](const auto &value) { return value.id == definition.garrison_id; });
    if (camp == catalog.garrisons.end())
      fail("UnknownGarrison", definition.garrison_id);
    StrongholdRecord hold;
    hold.id = definition.id;
    hold.owner_organization_id = camp->organization_id;
    state->strongholds.push_back(std::move(hold));
  }
  return state;
}
// 庄园初始化只写设施、任命和容量，不产生一期粮食或消费初始供养。
std::shared_ptr<const EstateState> seed_estates(CommonState &common,
                                                const EstateCatalog &catalog) {
  auto state = checkpoint_make_shared<EstateState>();
  for (const auto &row : catalog.estates.rows()) {
    const auto &d = row.second;
    EstateRecord estate;
    estate.id = d.id;
    estate.owner_actor_id = d.owner_actor_id;
    estate.manager_actor_id = d.manager_actor_id;
    estate.resident_actor_ids = d.resident_actor_ids;
    estate.facilities = d.initial_facilities;
    estate.last_tick = common.identity.tick;
    for (const auto &actor : d.resident_actor_ids)
      estate.duties.insert(actor, "idle");
    for (const auto &duty : d.initial_duties.rows())
      estate.duties.set(duty.first, duty.second);
    estate.duties.set(d.manager_actor_id, "idle");
    auto *inventory = common.inventories.find(d.inventory_id);
    if (!inventory || !common.wallets.find(d.wallet_id))
      fail("EstateAssetsMissing", d.id);
    const auto capacity = d.base_capacity + d.granary_capacity_per_level *
                                                estate.facilities.granary;
    if (capacity < 0 || capacity > 2147483647)
      fail("EstateCapacityOverflow", d.id);
    inventory->capacity = capacity;
    state->estates.insert(d.id, std::move(estate));
  }
  return state;
}
} // namespace
namespace {
// 原选型正文只有这一份；continuation
// 在原临时拥有者析构前执行，保持准备次序与寿命。
template <class Result, class Consume>
Outcome<Result> with_native_catalog_identity(const BoundaryValue &catalog,
                                             const StableId &scenario_id,
                                             Consume &&consume) {
  try {
    using namespace boundary;
    if (!get_bool(catalog, "ok", "catalog"))
      fail("InvalidCatalog", "catalog");
    const auto &scenario =
        field(field(field(catalog, "data", "catalog"), "scenarios", "data"),
              scenario_id, "scenarios");
    const auto &dynamic = field(scenario, "dynamic_world", scenario_id);
    if (get_string(dynamic, "profile", scenario_id) != "local_society_v1")
      unsupported("UnsupportedNativeProfile", scenario_id);
    CatalogIdentity admitted;
    admitted.content_id = get_string(catalog, "content_id", "catalog");
    admitted.content_version =
        get_string(catalog, "content_version", "catalog");
    admitted.content_digest = get_string(catalog, "content_digest", "catalog");
    admitted.schema_sha256 = get_string(catalog, "schema_sha256", "catalog");
    admitted.ruleset_id =
        checkpoint_decode_boundary_text("hanmo.native.local_society/1");
    if (const auto *pack = catalog.find("pack_set_digest"))
      admitted.pack_set_digest = string(*pack, "catalog/pack_set_digest");
    const bool population = dynamic.find("population_military") != nullptr,
               regional = dynamic.find("regional_life") != nullptr,
               integration = dynamic.find("siege_integration") != nullptr;
    const auto &organization = field(dynamic, "organization", "dynamic_world");
    const bool named =
        organization.find("named_membership_enabled") &&
        get_bool(organization, "named_membership_enabled", "organization");
    const auto &director_definition =
        field(dynamic, "director", "dynamic_world");
    auto director_subjects = decode_native_director_subject_selection(scenario);
    if (!director_subjects.ok)
      return {false, {}, std::move(director_subjects.error)};
    const auto &affairs_definition = field(dynamic, "affairs", "dynamic_world");
    const bool registry =
                   director_definition.find("registry_version") != nullptr,
               obligation_needs = affairs_definition.find(
                                      "obligation_needs_version") != nullptr,
               fiscal_needs =
                   affairs_definition.find("fiscal_need_policies") != nullptr,
               cohort_wages = affairs_definition.find(
                                  "cohort_wage_need_policies") != nullptr;
    if (registry &&
        get_int(director_definition, "registry_version", "director") != 1)
      fail("UnsupportedDirectorRegistryVersion", "director/registry_version");
    if (obligation_needs &&
        get_int(affairs_definition, "obligation_needs_version", "affairs") != 1)
      fail("UnsupportedObligationNeedsVersion",
           "affairs/obligation_needs_version");
    if (cohort_wages && get_int(field(affairs_definition,
                                      "cohort_wage_need_policies", "affairs"),
                                "version", "cohort_wage_need_policies") != 1)
      fail("UnsupportedCohortWageNeedVersion",
           "affairs/cohort_wage_need_policies/version");
    if (fiscal_needs &&
        get_int(field(affairs_definition, "fiscal_need_policies", "affairs"),
                "version", "fiscal_need_policies") != 1)
      fail("UnsupportedFiscalNeedVersion",
           "affairs/fiscal_need_policies/version");
    const auto &local_definition =
        field(dynamic, "local_economy", "dynamic_world");
    const bool management =
                   local_definition.find("production_management") != nullptr,
               household =
                   local_definition.find("household_provisioning") != nullptr;
    reserve_native_catalog_modules(admitted, 5);
    append_native_catalog_module(admitted, "dynamic_world", "local_society_v1");
    append_native_catalog_module(admitted, "local_economy",
                                 household    ? "5"
                                 : management ? "4"
                                 : named      ? "3"
                                 : population ? "2"
                                              : "1");
    append_native_catalog_module(admitted, "organization", named ? "3" : "2");
    append_native_catalog_module(admitted, "affairs",
                                 cohort_wages       ? "4"
                                 : fiscal_needs     ? "3"
                                 : obligation_needs ? "2"
                                                    : "1");
    append_native_catalog_module(
        admitted, "director",
        director_subjects.value.physical_version
            ? std::to_string(director_subjects.value.physical_version)
        : registry ? "4"
        : named    ? "3"
        : regional ? "2"
                   : "1");
    if (population) {
      auto decoded = decode_population_military_catalog(
          field(dynamic, "population_military", "dynamic_world"));
      if (!decoded.ok)
        return {false, {}, decoded.error};
      auto selected =
          selected_population_military_version(decoded.value, integration);
      if (!selected.ok)
        return {false, {}, selected.error};
      append_native_catalog_module(admitted, "population_military",
                                   std::to_string(selected.value));
    }
    if (regional)
      append_native_catalog_module(admitted, "regional_life", "1");
    if (integration)
      append_native_catalog_module(admitted, "siege_integration", "1");
    if (named)
      append_native_catalog_module(admitted, "named_membership", "1");
    // 与冷目录和GD身份同序；显式块须先通过原版本/启用守卫。
    if (const auto *definition = dynamic.find("diplomacy")) {
      auto diplomatic = decode_diplomacy_catalog(*definition);
      if (!diplomatic.ok)
        return {false, {}, diplomatic.error};
      auto valid = validate_diplomacy_runtime_profile(diplomatic.value);
      if (!valid.ok)
        return {false, {}, valid.error};
      append_native_catalog_module(admitted, "diplomacy",
                                   std::to_string(diplomatic.value.version));
      append_native_catalog_module(admitted, "envoy_tasks",
                                   std::to_string(diplomatic.value.version));
    }
    auto political_body = read_political_author_body_version(scenario);
    if (!political_body.ok)
      return {false, {}, std::move(political_body.error)};
    if (dynamic.find("politics"))
      append_native_catalog_module(admitted, "politics",
                                   std::to_string(political_body.value));
    if (dynamic.find("political_fiscal"))
      append_native_catalog_module(admitted, "political_fiscal",
                                   std::to_string(political_body.value));
    if (dynamic.find("treaty_passage"))
      append_native_catalog_module(admitted, "treaty_passage", "1");
    if (dynamic.find("political_military_appointments"))
      append_native_catalog_module(admitted, military_officer_political_module,
                                   "1");
    if (dynamic.find("political_field_appointments"))
      append_native_catalog_module(
          admitted, military_field_command_political_module, "1");
    if (dynamic.find("political_occupation_fiscal")) {
      append_native_catalog_module(admitted, "political_occupation_fiscal",
                                   "1");
      append_native_catalog_module(admitted, "occupation_fiscal_settlement",
                                   "1");
    }
    if (const auto *config = dynamic.find("political_takeover")) {
      const auto version = get_int(*config, "version", "political_takeover");
      if ((version != 1 && version != 2) || version != political_body.value)
        fail("PoliticalTakeoverAuthorVersionMismatch",
             "dynamic_world/political_takeover");
      auto parsed = version == 2 ? decode_political_takeover_catalog2(*config)
                                 : decode_political_takeover_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "political_takeover",
                                   std::to_string(version));
      append_native_catalog_module(admitted, "economic_takeover_execution",
                                   "1");
      append_native_catalog_module(
          admitted, "task_takeover_execution",
          std::to_string(parsed.value.task_execution_version));
    }
    if (management)
      append_native_catalog_module(admitted, "production_management", "1");
    if (household)
      append_native_catalog_module(admitted, "household_provisioning", "1");
    if (dynamic.find("fiscal_settlement"))
      append_native_catalog_module(admitted, "fiscal_settlement", "1");
    if (const auto *config = dynamic.find("fiscal_taxpayer_continuity")) {
      auto valid = decode_fiscal_taxpayer_continuity_profile(*config);
      if (!valid.ok)
        return {false, {}, valid.error};
      append_native_catalog_module(admitted, fiscal_taxpayer_continuity_module,
                                   "1");
      auto layout = validate_fiscal_taxpayer_continuity_layout(
          true, admitted.module_versions);
      if (!layout.ok)
        return {false, {}, layout.error};
    }
    if (read_politics_governance_profile(dynamic))
      for (const auto *name :
           {"politics_governance", "governance_tasks", "public_relief"})
        append_native_catalog_module(admitted, name, "1");
    if (registry)
      append_native_catalog_module(admitted, "director_registry", "1");
    if (director_subjects.value.subjects.enabled)
      append_native_catalog_module(admitted, "director_subjects", "2");
    // Director首次解码必须取得真实完整来访目录；这些只读目录不分配ID或改变旧身份顺序。
    const auto &reception_data = field(catalog, "data", "catalog");
    NativeCatalogCapabilities required_capabilities(reception_data);
    auto visit_reception =
        decode_visit_reception_profile(scenario, required_capabilities());
    if (!visit_reception.ok)
      return {false, {}, visit_reception.error};
    std::shared_ptr<const VisitReceptionCatalog> reception_catalog;
    if (visit_reception.value) {
      auto messages_profile =
          decode_visit_message_profile(scenario, required_capabilities());
      if (!messages_profile.ok)
        return {false, {}, messages_profile.error};
      if (!messages_profile.value)
        return {
            false, {}, common_failure("VisitReceptionProfileMismatch").error};
      auto reception_common = decode_common_catalog(reception_data);
      if (!reception_common.ok)
        return {false, {}, reception_common.error};
      auto reception_interaction = decode_interaction_catalog(
          reception_data, checkpoint_make_shared<const CommonCatalog>(
                              std::move(reception_common.value)));
      if (!reception_interaction.ok)
        return {false, {}, reception_interaction.error};
      auto reception_messages = decode_visit_message_catalog(
          field(scenario, "visit_messages", "scenario"),
          reception_interaction.value);
      if (!reception_messages.ok)
        return {false, {}, reception_messages.error};
      auto parsed_reception = decode_visit_reception_catalog(
          field(scenario, "visit_reception_intents", "scenario"),
          reception_messages.value, reception_interaction.value);
      if (!parsed_reception.ok)
        return {false, {}, parsed_reception.error};
      reception_catalog = checkpoint_make_shared<const VisitReceptionCatalog>(
          std::move(parsed_reception.value));
    }
    auto attendance = decode_personal_appointment_attendance_profile(
        scenario, required_capabilities());
    if (!attendance.ok)
      return {false, {}, attendance.error};
    if (attendance.value) {
      auto checked = validate_personal_appointment_attendance_author_source(
          scenario, reception_data);
      if (!checked.ok)
        return {false, {}, checked.error};
    }
    auto agency_intents_profile =
        decode_political_governance_agency_intent_profile(
            scenario, required_capabilities());
    if (!agency_intents_profile.ok)
      return {false, {}, agency_intents_profile.error};
    auto resignation_responses_profile =
        decode_political_office_resignation_response_profile(
            scenario, required_capabilities());
    if (!resignation_responses_profile.ok)
      return {false, {}, resignation_responses_profile.error};
    auto response_director = decode_director_catalog(
        director_definition, regional, named,
        dynamic.find("politics") != nullptr,
        dynamic.find("diplomacy") != nullptr,
        dynamic.find("diplomacy")
            ? get_int(*dynamic.find("diplomacy"), "version", "diplomacy")
            : 1,
        dynamic.find("treaty_passage") != nullptr,
        DirectorMilitaryAppointmentProfile{
            dynamic.find("political_military_appointments") != nullptr,
            dynamic.find("political_field_appointments") != nullptr},
        DirectorSupplyRenewalProfile{dynamic.find("economic_supply_policies") !=
                                     nullptr},
        reception_catalog,
        DirectorShopRegistryProfile{
            dynamic.find("political_shop_registry_successions") != nullptr},
        attendance.value,
        DirectorGovernanceIntentProfile{
            read_politics_governance_profile(dynamic)},
        DirectorGovernanceAgencyProfile{
            agency_intents_profile.value.has_value()},
        DirectorOfficeResignationResponseProfile{
            resignation_responses_profile.value.has_value()},
        director_subjects.value.subjects);
    if (!response_director.ok)
      return {false, {}, response_director.error};
    if (response_director.value.diplomatic_response)
      append_native_catalog_module(admitted, "diplomatic_response", "1");
    if (response_director.value.authored_task_continuation)
      append_native_catalog_module(
          admitted, "authored_task_continuation",
          std::to_string(
              response_director.value.authored_task_continuation->version));
    if (obligation_needs)
      append_native_catalog_module(admitted, "obligation_needs", "1");
    if (fiscal_needs)
      append_native_catalog_module(admitted, "fiscal_need_policies", "1");
    if (const auto *needs = dynamic.find("occupation_fiscal_needs")) {
      auto parsed = decode_occupation_fiscal_need_profile(*needs);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      if (!fiscal_needs || !dynamic.find("political_occupation_fiscal"))
        fail("OccupationFiscalNeedDependenciesRequired",
             "dynamic_world/occupation_fiscal_needs");
      append_native_catalog_module(admitted, occupation_fiscal_need_module,
                                   "1");
    }
    if (cohort_wages)
      append_native_catalog_module(admitted, "cohort_wage_need_policies", "1");
    if (regional)
      if (const auto *outcomes = field(dynamic, "regional_life", scenario_id)
                                     .find("outcome_topics"))
        if (!array(*outcomes, "outcome_topics").empty())
          append_native_catalog_module(admitted, "regional_outcome", "1");
    // 实际课程政策与选中情景各自进入身份，不提高旧目录的全局能力摘要。
    auto innate =
        decode_innate_learning_profile(scenario, required_capabilities());
    if (!innate.ok)
      return {false, {}, innate.error};
    const auto selected_roles = strings(
        field(field(scenario, "enabled", "scenario"), "roles", "enabled"),
        "enabled/roles");
    bool segmented = false;
    for (const auto &role :
         object(field(field(catalog, "data", "catalog"), "roles", "data"),
                "roles"))
      if (const auto *courses = role.second.find("skill_courses"))
        for (const auto &course : array(*courses, "skill_courses"))
          if (const auto *policy = course.find("settlement_policy")) {
            const auto selected = string(*policy, "settlement_policy");
            if (selected == "innate_per_lesson_v1") {
              if (std::find(selected_roles.begin(), selected_roles.end(),
                            role.first) == selected_roles.end())
                continue;
              if (!innate.value)
                fail("InnateLearningProfileRequired", "settlement_policy");
            } else if (selected != "per_lesson_v1")
              fail("InvalidStudySettlementPolicy", "settlement_policy");
            segmented = true;
          }
    if (segmented)
      append_native_catalog_module(admitted, "study_segments", "1");
    {
      auto selected_plan = decode_player_plan_profile_version(scenario);
      if (!selected_plan.ok)
        return {false, {}, selected_plan.error};
      if (selected_plan.value)
        append_native_catalog_module(admitted, "player_plan",
                                     std::to_string(selected_plan.value));
    }
    if (native_existing_actor_policy(
            get_string(dynamic, "founder_policy", scenario_id)))
      append_native_catalog_module(
          admitted, "selected_existing_actor",
          get_string(dynamic, "founder_policy", scenario_id) ==
                  "selected_existing_actor_roster_v1"
              ? "2"
              : "1");
    if (native_extended_skills(scenario))
      append_native_catalog_module(admitted, "skills_extended", "1");
    if (const auto *cards = scenario.find("growth_cards")) {
      if (get_string(*cards, "profile", "growth_cards") !=
          "learned_ability_cards_v1")
        unsupported("UnsupportedCardsProfile", "growth_cards/profile");
      append_native_catalog_module(admitted, "growth_cards", "1");
    }
    auto responses = decode_task_offer_response_profile_version(
        scenario, required_capabilities());
    if (!responses.ok)
      return {false, {}, responses.error};
    if (responses.value)
      append_native_catalog_module(admitted, "task_offer_responses",
                                   std::to_string(responses.value));
    auto advances =
        decode_task_personal_advance_profile(scenario, required_capabilities());
    if (!advances.ok)
      return {false, {}, advances.error};
    if (advances.value)
      append_native_catalog_module(admitted, "task_personal_advances", "1");
    auto handoffs =
        decode_task_message_handoff_version(scenario, required_capabilities());
    if (!handoffs.ok)
      return {false, {}, handoffs.error};
    if (handoffs.value)
      append_native_catalog_module(admitted, "task_message_handoffs",
                                   std::to_string(handoffs.value));
    auto invitations =
        decode_party_invitation_profile(scenario, required_capabilities());
    if (!invitations.ok)
      return {false, {}, invitations.error};
    if (invitations.value)
      append_native_catalog_module(admitted, "party_invitations", "1");
    auto recruitment = decode_party_recruitment_consent_profile(
        scenario, required_capabilities());
    if (!recruitment.ok)
      return {false, {}, recruitment.error};
    if (recruitment.value)
      append_native_catalog_module(admitted, "party_recruitment_consents", "1");
    if (const auto *cards = scenario.find("actor_card_unlocks")) {
      const auto profile = get_string(*cards, "profile", "actor_card_unlocks");
      if (profile != "completed_procurement_actor_cards_v1" &&
          profile != "completed_task_actor_cards_v2")
        unsupported("UnsupportedActorStartProfile",
                    "actor_card_unlocks/profile");
      append_native_catalog_module(
          admitted, "actor_card_unlocks",
          profile == "completed_task_actor_cards_v2" ? "2" : "1");
    }
    if (const auto *requests = scenario.find("teaching_requests")) {
      if (get_string(*requests, "profile", "teaching_requests") !=
          "authored_teaching_requests_v1")
        unsupported("UnsupportedTeachingRequestsProfile",
                    "teaching_requests/profile");
      append_native_catalog_module(admitted, "teaching_requests", "1");
    }
    if (const auto *requests = scenario.find("study_teaching_requests")) {
      if (get_string(*requests, "profile", "study_teaching_requests") !=
          "authored_study_teaching_requests_v1")
        unsupported("UnsupportedStudyTeachingProfile",
                    "study_teaching_requests/profile");
      append_native_catalog_module(admitted, "study_teaching_requests", "1");
    }
    auto military_requests = decode_military_request_policy(
        field(catalog, "data", "catalog"), scenario_id);
    if (!military_requests.ok)
      return {false, {}, military_requests.error};
    if (military_requests.value)
      append_native_catalog_module(admitted, "military_requests", "1");
    if (const auto *cards = scenario.find("insight_cards")) {
      if (get_string(*cards, "profile", "insight_cards") !=
          "completed_camp_training_insights_v1")
        unsupported("UnsupportedInsightCardsProfile", "insight_cards/profile");
      append_native_catalog_module(admitted, "growth_insights", "1");
    }
    auto naval_military =
        decode_military_catalog(field(catalog, "data", "catalog"), scenario_id);
    if (!naval_military.ok)
      return {false, {}, naval_military.error};
    if (naval_military.value.officer_assignment_policy)
      append_native_catalog_module(admitted, military_officer_module, "1");
    if (naval_military.value.field_command_policy)
      append_native_catalog_module(admitted, military_field_command_module,
                                   "1");
    if (naval_military.value.recovery_policy)
      append_native_catalog_module(admitted, military_recovery_module, "1");
    if (naval_military.value.supply_attrition_policy)
      append_native_catalog_module(admitted, military_supply_attrition_module,
                                   "1");
    if (naval_military.value.rout_retreat_policy)
      append_native_catalog_module(admitted, military_rout_module, "1");
    if (naval_military.value.command_shock_policy)
      append_native_catalog_module(admitted, military_command_shock_module,
                                   "1");
    if (naval_military.value.garrison_rest_policy)
      append_native_catalog_module(admitted, military_garrison_rest_module,
                                   "1");
    if (naval_military.value.combat_condition_policy)
      append_native_catalog_module(admitted, military_combat_conditions_module,
                                   "1");
    if (naval_military.value.commanded_damage_policy)
      append_native_catalog_module(admitted, military_commanded_damage_module,
                                   "1");
    if (naval_military.value.sequential_round_policy)
      append_native_catalog_module(admitted, military_sequential_module, "1");
    if (naval_military.value.field_terrain_policy)
      append_native_catalog_module(admitted, military_field_terrain_module,
                                   "1");
    if (naval_military.value.command_resources_policy)
      append_native_catalog_module(admitted, military_command_resources_module,
                                   "1");
    if (naval_military.value.battle_catalog.tactic_learning)
      append_native_catalog_module(admitted, military_tactic_learning_module,
                                   "1");
    if (naval_military.value.siege_engineering_policy)
      append_native_catalog_module(admitted, military_siege_engineering_module,
                                   "1");
    auto naval =
        decode_naval_combat_profile(field(catalog, "data", "catalog"), scenario,
                                    naval_military.value.battle_catalog);
    if (!naval.ok)
      return {false, {}, naval.error};
    if (naval.value && naval.value->catalog.repair_policy)
      append_native_catalog_module(admitted, "naval_repairs", "1");
    if (naval.value && naval.value->catalog.repair_charge_policy)
      append_native_catalog_module(admitted, "naval_repair_charges", "1");
    if (naval.value && naval.value->catalog.navigation_policy)
      append_native_catalog_module(admitted, "naval_navigation_wear", "1");
    if (naval.value && naval.value->catalog.combat_policy &&
        naval.value->catalog.combat_policy->combat.tactical_navigation)
      append_native_catalog_module(admitted, "naval_tactical_navigation", "1");
    if (naval.value && naval.value->catalog.combat_policy &&
        naval.value->catalog.combat_policy->combat.gunnery)
      append_native_catalog_module(admitted, "naval_gunnery", "1");
    if (naval.value)
      append_native_catalog_module(
          admitted, "military_naval",
          std::to_string(naval_codec::version(naval.value->catalog)));
    if (const auto *control = scenario.find("household_control")) {
      auto parsed = decode_household_control_catalog(*control);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "household_control", "1");
    }
    if (const auto *managed = dynamic.find("household_provision_management")) {
      if (!scenario.find("household_control") || !household)
        return {
            false,
            {},
            common_failure("HouseholdProvisionManagementDependencies").error};
      auto parsed = decode_household_provision_management_catalog(*managed);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "household_provision_management",
                                   "1");
    }
    auto encoded = render_boundary_json(catalog);
    if (!encoded.ok)
      return {false, {}, encoded.error};
    admitted.artifact_sha256 = picosha2::hash256_hex_string(encoded.value);
    if (scenario.find("character_creation"))
      append_native_catalog_module(admitted, "character_creation", "1");
    if (const auto *config = dynamic.find("production_disruptions")) {
      const auto &caps = required_capabilities();
      if (std::find(caps.begin(), caps.end(), "production_disruptions_v1") ==
          caps.end())
        unsupported("ProductionDisruptionCapabilityRequired",
                    "dynamic_world/production_disruptions");
      auto parsed = decode_production_disruption_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      if (parsed.value.profile != production_disruption_profile ||
          parsed.value.combination_policy != production_disruption_combination)
        return {false,
                {},
                common_failure("InvalidProductionDisruptionCatalog").error};
      append_native_catalog_module(admitted, "production_disruptions", "1");
    }
    if (const auto *config = scenario.find("adult_adoption")) {
      auto parsed = decode_adult_adoption_request_catalog(
          *config, field(field(catalog, "data", "catalog"),
                         "adult_adoption_requests", "data"));
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "adult_adoption", "1");
    }
    if (const auto *config = scenario.find("family_life")) {
      auto parsed = decode_native_family_life_policy(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(
          admitted, "family_life",
          std::to_string(parsed.value.selected_version));
    }
    if (const auto *config = scenario.find("family_legacy")) {
      const bool biological =
          scenario.find("family_biological_legacy") != nullptr;
      const bool collateral =
          scenario.find("family_collateral_legacy") != nullptr;
      const auto &data = field(catalog, "data", "catalog");
      BoundaryValue empty{BoundaryValue::Dict{}};
      const auto *definitions = data.find("legacy_control_consent_requests");
      if (!definitions && !biological && !collateral)
        fail("MissingField", "legacy_control_consent_requests");
      auto parsed =
          collateral ? decode_legacy_control_consent_catalog_v3(
                           *config, definitions ? *definitions : empty)
          : biological
              ? decode_legacy_control_consent_catalog_v2(
                    *config, definitions ? *definitions : empty)
              : decode_legacy_control_consent_catalog(*config, *definitions);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "family_legacy",
                                   std::to_string(parsed.value.version));
    }
    if (const auto *config = scenario.find("family_bequest")) {
      auto policy = decode_family_bequest_policy(*config);
      if (!policy.ok)
        return {false, {}, policy.error};
      append_native_catalog_module(admitted, "family_bequest", "1");
    }
    auto notices =
        decode_task_message_notice_catalog(scenario, required_capabilities());
    if (!notices.ok)
      return {false, {}, notices.error};
    if (notices.value)
      append_native_catalog_module(admitted, "task_message_notices", "1");
    if (const auto *config = scenario.find("family_inheritance")) {
      auto policy = decode_family_inheritance_policy(*config);
      if (!policy.ok)
        return {false, {}, policy.error};
      append_native_catalog_module(admitted, "family_inheritance", "1");
    }
    if (const auto *config = scenario.find("family_marriage")) {
      auto policy = decode_marriage_source_catalog(*config);
      if (!policy.ok)
        return {false, {}, policy.error};
      append_native_catalog_module(admitted, "family_marriage", "1");
    }
    auto static_assets =
        decode_task_static_asset_profile(scenario, required_capabilities());
    if (!static_assets.ok)
      return {false, {}, static_assets.error};
    if (static_assets.value)
      append_native_catalog_module(admitted, "task_static_assets", "1");
    if (const auto *config = dynamic.find("estate_employment")) {
      ScopedCheckpointJsonDecode scope(
          CheckpointCatalogJsonKind::estate_employment);
      auto parsed = decode_estate_employment_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "estate_employment", "1");
    }
    if (const auto *config = dynamic.find("economic_supply_policies")) {
      auto parsed = decode_economic_supply_policy_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "economic_supply_policies", "1");
      append_native_catalog_module(admitted, "political_supply_adoptions", "1");
    }
    auto supply_layout = validate_economic_supply_layout(
        dynamic.find("economic_supply_policies") != nullptr,
        admitted.module_versions);
    if (!supply_layout.ok)
      return {false, {}, supply_layout.error};
    if (response_director.value.supply_renewals)
      append_native_catalog_module(admitted, "political_supply_renewals", "1");
    if (const auto *config = scenario.find("voluntary_succession")) {
      const auto &data = field(catalog, "data", "catalog");
      const auto *definitions = data.find("succession_consent_requests");
      BoundaryValue empty{BoundaryValue::Dict{}};
      const bool living = get_string(*config, "profile", "") ==
                          "living_accepted_family_paths_v2";
      if (living != (scenario.find("family_voluntary_paths") != nullptr))
        fail("VoluntaryPathSuccessionPairRequired", "scenario");
      auto parsed =
          living ? decode_living_succession_catalog(
                       *config, definitions ? *definitions : empty)
                 : decode_succession_consent_catalog(
                       *config,
                       field(data, "succession_consent_requests", "data"));
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "voluntary_succession",
                                   living ? "2" : "1");
    }
    if (bool(scenario.find("family_birth")) !=
        bool(scenario.find("family_lineage")))
      fail("FamilyBirthProfileMismatch", "scenario");
    if (const auto *config = scenario.find("family_birth")) {
      auto birth = decode_native_family_birth_catalog(scenario, admitted);
      if (!birth.ok)
        return {false, {}, birth.error};
      auto lineage = decode_family_lineage_catalog(
          field(scenario, "family_lineage", "scenario"));
      if (!lineage.ok)
        return {false, {}, lineage.error};
      append_native_catalog_module(admitted, "family_lineage", "1");
      append_native_catalog_module(
          admitted, "family_birth",
          std::to_string(birth.value.selected_version));
    }
    auto visit_messages =
        decode_visit_message_profile_version(scenario, required_capabilities());
    if (!visit_messages.ok)
      return {false, {}, visit_messages.error};
    if (visit_messages.value)
      append_native_catalog_module(admitted, "visit_messages",
                                   std::to_string(visit_messages.value));
    if (innate.value)
      append_native_catalog_module(admitted, "family_innate_learning", "1");
    if (visit_reception.value)
      append_native_catalog_module(admitted, "visit_reception_intents", "1");
    if (visit_reception.value)
      append_native_catalog_module(admitted, "visit_reception_claims", "1");
    if (const auto *config = scenario.find("family_care_relief")) {
      auto care = decode_child_care_relief_catalog(*config);
      if (!care.ok)
        return {false, {}, care.error};
      if (!scenario.find("family_birth") || !scenario.find("family_lineage") ||
          !scenario.find("family_life"))
        fail("ChildCareReliefProfileRequired", "scenario");
      append_native_catalog_module(admitted, "family_care_relief", "1");
    }
    if (const auto *market = dynamic.find("growth_market_history")) {
      const auto version = get_int(*market, "version", "growth_market_history");
      auto selected = version == 2
                          ? decode_growth_market_history_selection_v2(*market)
                          : decode_growth_market_history_selection(*market);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      append_native_catalog_module(admitted, "growth_market_history",
                                   std::to_string(version));
    }
    if (const auto *law = dynamic.find("political_shop_licensing")) {
      auto selected = decode_political_shop_licensing_author_profile(*law);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      append_native_catalog_module(
          admitted, "political_shop_licensing",
          std::to_string(selected.value.source_version));
    }
    if (const auto *shops = dynamic.find("private_shops")) {
      auto selected = decode_private_shop_profile(*shops);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      append_native_catalog_module(admitted, "private_shops",
                                   std::to_string(selected.value.version));
    }
    if (const auto *profile = scenario.find("task_shop_order_profile")) {
      if (string(*profile, "task_shop_order_profile") !=
          "private_shop_orders_v1")
        fail("UnsupportedTaskShopOrderProfile", "task_shop_order_profile");
      append_native_catalog_module(admitted, "task_shop_orders", "1");
    }
    auto task_extensions =
        decode_task_extension_profile(scenario, required_capabilities());
    if (!task_extensions.ok)
      return {false, {}, task_extensions.error};
    if (task_extensions.value)
      append_native_catalog_module(admitted, "task_extensions", "1");
    if (const auto *registry =
            dynamic.find("political_shop_registry_successions")) {
      auto parsed = decode_political_shop_registry_catalog(*registry);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      if (!parsed.value.enabled || !dynamic.find("political_shop_licensing") ||
          !dynamic.find("private_shops") || !dynamic.find("political_takeover"))
        fail("PoliticalShopRegistrySuccessionDependenciesRequired",
             "dynamic_world/political_shop_registry_successions");
      append_native_catalog_module(admitted,
                                   "political_shop_registry_successions", "1");
    }
    if (response_director.value.shop_registry_intents)
      append_native_catalog_module(admitted, "political_shop_registry_intents",
                                   "1");
    auto appointments = decode_personal_appointment_profile_version(
        scenario, required_capabilities());
    if (!appointments.ok)
      return {false, {}, appointments.error};
    if (appointments.value)
      append_native_catalog_module(admitted, "personal_appointments",
                                   std::to_string(appointments.value));
    if (const auto *dynamic = scenario.find("dynamic_world"))
      if (dynamic->find("personal_injury_recovery"))
        append_native_catalog_module(admitted, "personal_injury_recovery", "1");
    if (attendance.value)
      append_native_catalog_module(admitted, "personal_appointment_attendance",
                                   "1");
    auto legal_receipts = decode_political_shop_licensing_receipt_profile(
        scenario, required_capabilities());
    if (!legal_receipts.ok)
      return {false, {}, legal_receipts.error};
    if (legal_receipts.value)
      append_native_catalog_module(admitted,
                                   "political_shop_licensing_receipts", "1");
    auto liquidations = decode_private_shop_liquidation_profile(
        scenario, required_capabilities());
    if (!liquidations.ok)
      return {false, {}, liquidations.error};
    if (liquidations.value) {
      append_native_catalog_module(admitted, "private_shop_liquidations", "1");
      append_native_catalog_module(admitted,
                                   "task_shop_order_estate_cancellations", "1");
    }
    auto liquidation_pair = validate_private_shop_liquidation_pair(
        bool(liquidations.value), admitted.module_versions);
    if (!liquidation_pair.ok)
      return {false, {}, liquidation_pair.error};
    if (const auto *config = scenario.find("family_apprenticeship")) {
      auto parsed = decode_apprenticeship_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      const auto &caps = required_capabilities();
      if (std::find(caps.begin(), caps.end(), "family_apprenticeship_v1") ==
          caps.end())
        fail("ApprenticeshipCapabilityRequired",
             "scenario/family_apprenticeship");
      if (!scenario.find("family_life"))
        fail("ApprenticeshipDependenciesRequired",
             "scenario/family_apprenticeship");
      append_native_catalog_module(admitted, "family_apprenticeship", "1");
    }
    auto continued = decode_political_governance_continuation_profile(
        scenario, required_capabilities());
    if (!continued.ok)
      return {false, {}, continued.error};
    if (continued.value)
      append_native_catalog_module(admitted,
                                   "political_governance_continuations", "1");
    auto inquiries = decode_contact_inquiry_profile_version(
        scenario, required_capabilities());
    if (!inquiries.ok)
      return {false, {}, inquiries.error};
    if (inquiries.value)
      append_native_catalog_module(admitted, "contact_inquiries",
                                   std::to_string(inquiries.value));
    if (response_director.value.governance_intents)
      append_native_catalog_module(admitted, "political_governance_intents",
                                   "1");
    auto visit_replies =
        decode_visit_reply_profile_version(scenario, required_capabilities());
    if (!visit_replies.ok)
      return {false, {}, visit_replies.error};
    if (visit_replies.value)
      append_native_catalog_module(admitted, "visit_message_replies",
                                   std::to_string(visit_replies.value));
    if (const auto *config = scenario.find("family_disciple_legacy")) {
      auto parsed = decode_disciple_control_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      const auto &caps = required_capabilities();
      if (std::find(caps.begin(), caps.end(), "family_disciple_legacy_v1") ==
          caps.end())
        fail("DiscipleControlCapabilityRequired",
             "scenario/family_disciple_legacy");
      if (!scenario.find("family_life") || !scenario.find("family_legacy") ||
          !scenario.find("family_apprenticeship"))
        fail("DiscipleControlDependenciesRequired",
             "scenario/family_disciple_legacy");
      append_native_catalog_module(admitted, "family_disciple_legacy", "1");
    }
    if (visit_reply_authored_version(visit_replies.value))
      append_native_catalog_module(admitted, "visit_reply_intents", "1");
    auto invitation_replies = decode_governance_invitation_reply_profile(
        scenario, required_capabilities());
    if (!invitation_replies.ok)
      return {false, {}, invitation_replies.error};
    if (invitation_replies.value)
      append_native_catalog_module(admitted, governance_invitation_reply_module,
                                   "1");
    auto invitation_modules = validate_governance_invitation_reply_modules(
        admitted.module_versions, invitation_replies.value);
    if (!invitation_modules.ok)
      return {false, {}, invitation_modules.error};
    auto daily_rent = decode_private_shop_daily_rent_profile(
        scenario, required_capabilities());
    if (!daily_rent.ok)
      return {false, {}, daily_rent.error};
    if (daily_rent.value)
      append_native_catalog_module(admitted, private_shop_daily_rent_module,
                                   "1");
    auto daily_pair = validate_private_shop_daily_rent_pair(
        bool(daily_rent.value), admitted.module_versions);
    if (!daily_pair.ok)
      return {false, {}, daily_pair.error};
    auto born_dialogues = decode_born_personal_dialogue_profile(
        scenario, required_capabilities());
    if (!born_dialogues.ok)
      return {false, {}, born_dialogues.error};
    if (born_dialogues.value)
      append_native_catalog_module(admitted, "born_personal_dialogues", "1");
    if (const auto *config = scenario.find("family_biological_legacy")) {
      auto parsed = decode_biological_control_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      const auto &caps = required_capabilities();
      if (std::find(caps.begin(), caps.end(), "family_biological_legacy_v1") ==
              caps.end() ||
          std::find(caps.begin(), caps.end(),
                    scenario.find("family_collateral_legacy")
                        ? "family_legacy_v3"
                        : "family_legacy_v2") == caps.end())
        fail("BiologicalControlCapabilityRequired",
             "scenario/family_biological_legacy");
      if (!scenario.find("family_legacy") || !scenario.find("family_life") ||
          !scenario.find("family_birth"))
        fail("BiologicalControlDependenciesRequired",
             "scenario/family_biological_legacy");
      append_native_catalog_module(admitted, "family_biological_legacy", "1");
    }
    auto successions =
        decode_political_succession_profile(scenario, required_capabilities());
    if (!successions.ok)
      return {false, {}, successions.error};
    if (successions.value)
      append_native_catalog_module(admitted, political_succession_domain, "1");
    auto succession_modules =
        political_succession_selected_version(admitted.module_versions);
    if (!succession_modules.ok)
      return {false, {}, succession_modules.error};
    auto visit_appointments = decode_visit_appointment_profile_version(
        scenario, required_capabilities());
    if (!visit_appointments.ok)
      return {false, {}, visit_appointments.error};
    if (visit_appointments.value) {
      append_native_catalog_module(admitted, "visit_appointment_invitations",
                                   std::to_string(visit_appointments.value));
      append_native_catalog_module(admitted, "visit_appointment_meetings",
                                   std::to_string(visit_appointments.value));
      if (scenario.find("visit_appointment_intents"))
        append_native_catalog_module(admitted, "visit_appointment_intents",
                                     "1");
    }
    auto handovers = decode_political_head_handover_profile(
        scenario, required_capabilities());
    if (!handovers.ok)
      return {false, {}, handovers.error};
    if (handovers.value)
      append_native_catalog_module(admitted, political_head_handover_domain,
                                   "1");
    auto handover_modules =
        political_head_handover_selected_version(admitted.module_versions);
    if (!handover_modules.ok)
      return {false, {}, handover_modules.error};
    auto shop_results = decode_private_shop_command_results_profile(
        scenario, required_capabilities());
    if (!shop_results.ok)
      return {false, {}, shop_results.error};
    if (shop_results.value)
      append_native_catalog_module(admitted,
                                   private_shop_command_results_module, "1");
    auto shop_results_pair = validate_private_shop_command_results_pair(
        bool(shop_results.value), bool(dynamic.find(private_shop_module)),
        admitted.module_versions);
    if (!shop_results_pair.ok)
      return {false, {}, shop_results_pair.error};
    auto written_attendance = decode_visit_appointment_attendance_profile(
        scenario, required_capabilities());
    if (!written_attendance.ok)
      return {false, {}, written_attendance.error};
    if (written_attendance.value)
      append_native_catalog_module(admitted, "visit_appointment_attendance",
                                   "1");
    auto lessons =
        decode_growth_tactic_lesson_profile(scenario, required_capabilities());
    if (!lessons.ok)
      return {false, {}, lessons.error};
    if (lessons.value)
      append_native_catalog_module(admitted, "growth_tactic_lessons", "1");
    auto rounds = decode_political_successor_round_profile(
        scenario, required_capabilities());
    if (!rounds.ok)
      return {false, {}, rounds.error};
    if (rounds.value)
      append_native_catalog_module(admitted, political_successor_rounds_domain,
                                   "1");
    auto round_modules =
        political_successor_round_selected_version(admitted.module_versions);
    if (!round_modules.ok)
      return {false, {}, round_modules.error};
    auto term_handovers = decode_political_term_handover_profile(
        scenario, required_capabilities());
    if (!term_handovers.ok)
      return {false, {}, term_handovers.error};
    if (term_handovers.value)
      append_native_catalog_module(admitted, political_term_handover_domain,
                                   "1");
    auto term_modules =
        political_term_handover_selected_version(admitted.module_versions);
    if (!term_modules.ok)
      return {false, {}, term_modules.error};
    auto message_receipts = decode_visit_message_receipt_profile_version(
        scenario, required_capabilities());
    if (!message_receipts.ok)
      return {false, {}, message_receipts.error};
    if (message_receipts.value)
      append_native_catalog_module(admitted, visit_message_receipt_module,
                                   std::to_string(message_receipts.value));
    auto message_receipt_modules =
        visit_message_receipt_selected_version(admitted.module_versions);
    if (!message_receipt_modules.ok)
      return {false, {}, message_receipt_modules.error};
    auto known_dialogues = decode_known_personal_dialogue_profile(
        scenario, required_capabilities());
    if (!known_dialogues.ok)
      return {false, {}, known_dialogues.error};
    if (known_dialogues.value)
      append_native_catalog_module(admitted, "known_personal_dialogues", "1");
    auto office_receptions = decode_political_office_reception_profile(
        scenario, required_capabilities());
    if (!office_receptions.ok)
      return {false, {}, office_receptions.error};
    if (office_receptions.value)
      append_native_catalog_module(admitted, political_office_reception_domain,
                                   "1");
    auto reception_modules =
        political_office_reception_selected_version(admitted.module_versions);
    if (!reception_modules.ok)
      return {false, {}, reception_modules.error};
    auto shop_contacts = decode_task_shop_order_contact_profile(
        scenario, required_capabilities());
    if (!shop_contacts.ok)
      return {false, {}, shop_contacts.error};
    if (shop_contacts.value)
      append_native_catalog_module(admitted, task_shop_order_contact_module,
                                   "1");
    auto contact_modules =
        task_shop_order_contact_selected_version(admitted.module_versions);
    if (!contact_modules.ok)
      return {false, {}, contact_modules.error};
    if (const auto *config = scenario.find("family_collateral_legacy")) {
      auto parsed = decode_collateral_control_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      const auto &caps = required_capabilities();
      if (std::find(caps.begin(), caps.end(), "family_collateral_legacy_v1") ==
              caps.end() ||
          std::find(caps.begin(), caps.end(), "family_legacy_v3") == caps.end())
        fail("CollateralControlCapabilityRequired",
             "scenario/family_collateral_legacy");
      if (!scenario.find("family_legacy") || !scenario.find("family_life") ||
          !scenario.find("family_lineage") || !known_dialogues.value)
        fail("CollateralControlDependenciesRequired",
             "scenario/family_collateral_legacy");
      append_native_catalog_module(admitted, "family_collateral_legacy", "1");
    }
    auto governance_agencies = decode_political_governance_agency_profile(
        scenario, required_capabilities());
    if (!governance_agencies.ok)
      return {false, {}, governance_agencies.error};
    if (governance_agencies.value) {
      append_native_catalog_module(
          admitted, political_governance_agency_domain,
          std::to_string(governance_agencies.value->version));
      if (agency_intents_profile.value)
        append_native_catalog_module(
            admitted, political_governance_agency_intents_module, "1");
    }
    auto agency_modules =
        political_governance_agency_selected_version(admitted.module_versions);
    if (!agency_modules.ok)
      return {false, {}, agency_modules.error};
    auto personal_outcomes =
        decode_personal_catalog(field(catalog, "data", "catalog"));
    if (!personal_outcomes.ok)
      return {false, {}, personal_outcomes.error};
    auto simultaneous = configure_personal_simultaneous_outcomes(
        field(catalog, "data", "catalog"), scenario, personal_outcomes.value);
    if (!simultaneous.ok)
      return {false, {}, simultaneous.error};
    if (!personal_outcomes.value.simultaneous_outcome_objectives.empty())
      append_native_catalog_module(admitted,
                                   personal_simultaneous_outcomes_module, "1");
    auto message_returns =
        decode_task_message_return_profile(scenario, required_capabilities());
    if (!message_returns.ok)
      return {false, {}, message_returns.error};
    if (message_returns.value)
      append_native_catalog_module(admitted, task_message_return_module, "1");
    auto return_modules =
        task_message_return_selected_version(admitted.module_versions);
    if (!return_modules.ok)
      return {false, {}, return_modules.error};
    if (const auto *config = scenario.find("family_voluntary_paths")) {
      const auto &caps = required_capabilities();
      if (std::find(caps.begin(), caps.end(), "family_voluntary_paths_v1") ==
          caps.end())
        fail("VoluntaryPathCapabilityRequired",
             "scenario/family_voluntary_paths");
      const auto *voluntary = scenario.find("voluntary_succession");
      if (!voluntary ||
          get_string(*voluntary, "profile", "") !=
              "living_accepted_family_paths_v2" ||
          !known_dialogues.value)
        fail("VoluntaryPathSourcePairRequired",
             "scenario/family_voluntary_paths");
      auto parsed = decode_voluntary_path_catalog(*config);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      append_native_catalog_module(admitted, "family_voluntary_paths", "1");
      auto personal_messages = decode_task_personal_message_profile(
          scenario, required_capabilities());
      if (!personal_messages.ok)
        return {false, {}, personal_messages.error};
      if (personal_messages.value)
        append_native_catalog_module(admitted, task_personal_message_module,
                                     "1");
      auto personal_reports = decode_task_personal_report_profile(
          scenario, required_capabilities());
      if (!personal_reports.ok)
        return {false, {}, personal_reports.error};
      if (personal_reports.value)
        append_native_catalog_module(admitted, task_personal_report_module,
                                     "1");
      auto personal_cancellations = decode_task_personal_cancellation_profile(
          scenario, required_capabilities());
      if (!personal_cancellations.ok)
        return {false, {}, personal_cancellations.error};
      if (personal_cancellations.value)
        append_native_catalog_module(admitted,
                                     task_personal_cancellation_module, "1");
      auto cancellation_modules =
          task_personal_cancellation_selected_version(admitted.module_versions);
      if (!cancellation_modules.ok)
        return {false, {}, cancellation_modules.error};
      auto report_modules =
          task_personal_report_selected_version(admitted.module_versions);
      if (!report_modules.ok)
        return {false, {}, report_modules.error};
      auto personal_modules =
          task_personal_message_selected_version(admitted.module_versions);
      if (!personal_modules.ok)
        return {false, {}, personal_modules.error};
      auto office_resignations = decode_political_office_resignation_profile(
          scenario, required_capabilities());
      if (!office_resignations.ok)
        return {false, {}, office_resignations.error};
      if (office_resignations.value) {
        append_native_catalog_module(
            admitted, political_office_resignation_module,
            std::to_string(office_resignations.value->version));
        if (resignation_responses_profile.value)
          append_native_catalog_module(
              admitted, political_office_resignation_responses_module, "1");
      }
      auto resignation_modules = political_office_resignation_selected_version(
          admitted.module_versions);
      if (!resignation_modules.ok)
        return {false, {}, resignation_modules.error};
      auto subordinate_appointments =
          decode_political_subordinate_appointment_profile(
              scenario, required_capabilities());
      if (!subordinate_appointments.ok)
        return {false, {}, subordinate_appointments.error};
      auto subordinate_intents = decode_political_subordinate_intent_profile(
          scenario, required_capabilities());
      if (!subordinate_intents.ok)
        return {false, {}, subordinate_intents.error};
      if (subordinate_appointments.value)
        append_native_catalog_module(
            admitted, political_subordinate_appointment_module, "1");
      if (subordinate_intents.value)
        append_native_catalog_module(
            admitted, political_subordinate_appointment_intents_module, "1");
      auto subordinate_version =
          political_subordinate_appointment_selected_version(
              admitted.module_versions);
      if (!subordinate_version.ok)
        return {false, {}, subordinate_version.error};
      auto personal_acceptances = decode_task_personal_acceptance_profile(
          scenario, required_capabilities());
      if (!personal_acceptances.ok)
        return {false, {}, personal_acceptances.error};
      if (personal_acceptances.value)
        append_native_catalog_module(admitted, task_personal_acceptance_module,
                                     "1");
      auto acceptance_modules =
          task_personal_acceptance_selected_version(admitted.module_versions);
      if (!acceptance_modules.ok)
        return {false, {}, acceptance_modules.error};
      auto dismissal_sources = decode_political_dismissal_source_profile(
          scenario, required_capabilities());
      if (!dismissal_sources.ok)
        return {false, {}, dismissal_sources.error};
      if (dismissal_sources.value)
        append_native_catalog_module(admitted,
                                     political_dismissal_sources_module, "1");
      auto dismissal_version =
          political_dismissal_source_selected_version(admitted.module_versions);
      if (!dismissal_version.ok)
        return {false, {}, dismissal_version.error};
    }
    auto innate_layout =
        validate_innate_learning_layout(innate.value, admitted.module_versions);
    if (!innate_layout.ok)
      return {false, {}, innate_layout.error};

    auto officer_pair =
        validate_military_siege_engineering_modules(admitted.module_versions);
    if (!officer_pair.ok)
      return {false, {}, officer_pair.error};
    auto household_catalogs =
        append_native_household_modules(scenario, admitted);
    if (!household_catalogs.ok)
      return {false, {}, std::move(household_catalogs.error)};
    auto face_module = append_native_face_to_face_module(scenario, admitted);
    if (!face_module.ok)
      return {false, {}, std::move(face_module.error)};
    auto occupancy =
        append_native_actor_social_occupancy_module(scenario, admitted);
    if (!occupancy.ok)
      return {false, {}, std::move(occupancy.error)};
    auto occupation =
        append_native_military_occupation_module(scenario, admitted);
    if (!occupation.ok)
      return {false, {}, std::move(occupation.error)};
    auto exact = decode_exact_time_profile(scenario);
    if (!exact.ok)
      return {false, {}, exact.error};
    if (exact.value) {
      auto selected = append_exact_time_modules(admitted, &*exact.value);
      if (!selected.ok)
        return {false, {}, selected.error};
    }
    auto shop_modules =
        append_native_private_shop_operation_modules(scenario, admitted);
    if (!shop_modules.ok)
      return {false, {}, std::move(shop_modules.error)};
    auto custody_module =
        append_native_private_shop_household_custody_module(scenario, admitted);
    if (!custody_module.ok)
      return {false, {}, custody_module.error};
    auto item_craft_modules =
        append_native_item_craft_modules(scenario, admitted);
    if (!item_craft_modules.ok)
      return {false, {}, std::move(item_craft_modules.error)};
    auto facility_modules =
        append_native_economic_facility_modules(scenario, admitted);
    if (!facility_modules.ok)
      return {false, {}, std::move(facility_modules.error)};
    auto military_module =
        append_native_military_state_modules(scenario, admitted);
    if (!military_module.ok)
      return {false, {}, std::move(military_module.error)};
    // 明选且真实启用的个人生命周期须登记原物理身份；不新增存档段。
    if (personal_outcomes.value.enabled &&
        scenario.find("personal_battle_lifecycle"))
      append_native_catalog_module(admitted, "personal", "1");
    auto personal_lifecycle =
        append_native_personal_battle_lifecycle_module(scenario, admitted);
    if (!personal_lifecycle.ok)
      return {false, {}, std::move(personal_lifecycle.error)};
    auto battle_lifecycle =
        append_native_military_battle_lifecycle_module(scenario, admitted);
    if (!battle_lifecycle.ok)
      return {false, {}, std::move(battle_lifecycle.error)};
    auto organization_module =
        append_native_organization_runtime_module(scenario, admitted);
    if (!organization_module.ok)
      return {false, {}, std::move(organization_module.error)};
    auto obligation_module =
        append_native_economic_obligation_module(scenario, admitted);
    if (!obligation_module.ok)
      return {false, {}, std::move(obligation_module.error)};
    auto population_module =
        append_native_born_population_module(scenario, admitted);
    if (!population_module.ok)
      return {false, {}, std::move(population_module.error)};
    auto control_module =
        append_native_control_continuation_module(scenario, admitted);
    if (!control_module.ok)
      return {false, {}, std::move(control_module.error)};
    auto interaction_response_module =
        append_native_interaction_response_module(scenario, admitted);
    if (!interaction_response_module.ok)
      return {false, {}, std::move(interaction_response_module.error)};
    auto age_module = append_native_life_age_module(scenario, admitted);
    if (!age_module.ok)
      return {false, {}, std::move(age_module.error)};
    auto task_execution_module =
        append_native_task_execution_module(scenario, admitted);
    if (!task_execution_module.ok)
      return {false, {}, std::move(task_execution_module.error)};
    auto task_work_module = append_native_task_work_module(scenario, admitted);
    if (!task_work_module.ok)
      return {false, {}, std::move(task_work_module.error)};
    auto governance_module =
        append_native_governance_program_modules(scenario, admitted);
    if (!governance_module.ok)
      return {false, {}, std::move(governance_module.error)};
    auto production_module =
        append_native_production_cycles_module(scenario, admitted);
    if (!production_module.ok)
      return {false, {}, std::move(production_module.error)};
    auto funding_module =
        append_native_private_transport_funding_module(scenario, admitted);
    if (!funding_module.ok)
      return {false, {}, std::move(funding_module.error)};
    auto transport_module = append_native_transport_modules(scenario, admitted);
    if (!transport_module.ok)
      return {false, {}, std::move(transport_module.error)};
    auto social_cut_module =
        append_native_social_source_modules(scenario, admitted);
    if (!social_cut_module.ok)
      return {false, {}, std::move(social_cut_module.error)};
    auto inspection_module =
        append_native_public_supply_inspection_module(scenario, admitted);
    if (!inspection_module.ok)
      return {false, {}, std::move(inspection_module.error)};
    if (const auto *selected = dynamic.find("field_battle")) {
      auto parameters = decode_field_battle_parameters(*selected);
      if (!parameters.ok)
        return {false, {}, parameters.error};
      append_native_catalog_module(admitted, "field_battle", "1");
    }
    auto boundary_module =
        append_native_movement_boundary_module(scenario, admitted);
    if (!boundary_module.ok)
      return {false, {}, std::move(boundary_module.error)};
    // 前置选择决定必备续行段及来源身份；不读取任何运行表或改作者输入。
    auto continuation_module =
        append_native_movement_boundary_continuation_module(scenario, admitted);
    if (!continuation_module.ok)
      return {false, {}, std::move(continuation_module.error)};
    auto local_move_module =
        append_native_local_move_module(scenario, admitted);
    if (!local_move_module.ok)
      return {false, {}, std::move(local_move_module.error)};
    // source1 的完整原身体门借 Field1 与 Personal lifecycle1；只在真实作者
    // 模块已登记、原 Personal 目录已解码后配对，不从空伤势表推导可用。
    auto injury_module = append_native_character_injury_module(
        scenario, admitted, exact.value ? &*exact.value : nullptr,
        personal_outcomes.value);
    if (!injury_module.ok)
      return {false, {}, std::move(injury_module.error)};
    auto relations_module = append_native_political_relations_modules(
        scenario, admitted, exact.value ? &*exact.value : nullptr);
    if (!relations_module.ok)
      return {false, {}, std::move(relations_module.error)};
    auto field_passage_modules = append_native_field_passage_modules(
        scenario, admitted, exact.value ? &*exact.value : nullptr);
    if (!field_passage_modules.ok)
      return {false, {}, std::move(field_passage_modules.error)};
    auto handoff_modules =
        append_native_group_member_handoff_modules(scenario, admitted);
    if (!handoff_modules.ok)
      return {false, {}, std::move(handoff_modules.error)};
    return consume(admitted, scenario, dynamic);
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace

// 宿主冷请求只读本次作者配置的真实选型，不创建世界，也不接受或修正调用者
// expected。
Outcome<CatalogIdentity>
read_native_catalog_identity(const BoundaryValue &catalog,
                             const StableId &scenario_id) {
  auto created =
      CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                  engineering_slot_capacity_profile());
  if (!created.ok)
    throw checkpoint_job_detail::FailureException{std::move(created.error)};
  auto &job = created.value;
  try {
    auto resource =
        checkpoint_memory_resource(job, CheckpointJobUse::candidate);
    if (!resource.ok)
      throw checkpoint_job_detail::FailureException{std::move(resource.error)};
    ScopedCheckpointMemory memory(resource.value);
    CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(), job);
    ScopedCheckpointBoundaryDecode fields(budget, job, resource.value);
    return with_native_catalog_identity<CatalogIdentity>(
        catalog, scenario_id,
        [](CatalogIdentity &admitted, const BoundaryValue &,
           const BoundaryValue &) -> Outcome<CatalogIdentity> {
          return {true, std::move(admitted), {}};
        });
  } catch (const std::bad_alloc &) {
    throw checkpoint_job_detail::FailureException{
        job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};
  } catch (const std::length_error &) {
    throw checkpoint_job_detail::FailureException{
        job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};
  }
}

// 恢复与新局仍先独立派生再严格核 expected；只有相同身份才能进入原冷目录投影。
Outcome<std::shared_ptr<const PreparedCatalog>> prepare_native_catalog(
    const BoundaryValue &catalog, const StableId &scenario_id,
    const CatalogIdentity &expected, const FieldBattleResourceRead *resources) {
  return with_native_catalog_identity<std::shared_ptr<const PreparedCatalog>>(
      catalog, scenario_id,
      [&](CatalogIdentity &admitted, const BoundaryValue &scenario,
          const BoundaryValue &dynamic)
          -> Outcome<std::shared_ptr<const PreparedCatalog>> {
        using namespace boundary;
        if (!same_original_catalog_identity(admitted, expected))
          fail("NativeCatalogIdentityMismatch",
               "request/expected_catalog_identity");
        NativeInitRequest projection;
        projection.scenario_id = scenario_id;
        auto effective = effective_catalog(catalog, projection);
        if (native_existing_actor_policy(
                get_string(dynamic, "founder_policy", scenario_id))) {
          auto actors_valid = validate_native_actor_choices(
              field(effective, "data", "catalog"), scenario);
          if (!actors_valid.ok)
            return {false, {}, actors_valid.error};
        }
        if (get_string(dynamic, "founder_policy", scenario_id) !=
            "created_character_v1") {
          auto appearance_valid = validate_native_appearance_choices(
              field(effective, "data", "catalog"), scenario);
          if (!appearance_valid.ok)
            return {false, {}, appearance_valid.error};
        }
        auto metadata = obj({{"ruleset_id", V{admitted.ruleset_id}},
                             {"scenario_id", V{scenario_id}}});
        // 在原 Complete 选型前装载同 admitted 的真实资源；不以空目录代替资源，
        // shared 拥有者仍沿原同 job 复制和析构，不保留宿主路径或借用 provider。
        std::shared_ptr<const FieldBattleCatalog> original_field;
        if (const auto *selected = dynamic.find("field_battle")) {
          if (!resources)
            return {false,
                    {},
                    common_failure("FieldTerrainMissing",
                                   "field_battle/resources",
                                   ErrorKind::unsupported_capability)
                        .error};
          auto terrain =
              prepare_field_battle_catalog(field(effective, "data", "catalog"),
                                           scenario_id, admitted, *resources);
          if (!terrain.ok)
            return {false, {}, terrain.error};
          original_field =
              copy_locked_native_catalog(terrain.value, admitted);
        }
        auto prepared = prepare_catalog_with_original_field(
            effective, metadata, std::move(original_field));
        if (!prepared.ok)
          return prepared;
        auto result = copy_locked_native_catalog(*prepared.value, admitted);
        auto custody_catalog =
            prepare_native_private_shop_household_custody_catalog(scenario,
                                                                  *result);
        if (!custody_catalog.ok)
          return {false, {}, custody_catalog.error};
        auto inspection_catalog =
            prepare_native_public_supply_inspection_catalog(scenario, *result);
        if (!inspection_catalog.ok)
          return {false, {}, std::move(inspection_catalog.error)};
        if (result->economic_facilities)
          result->economic_facilities = copy_locked_native_catalog(
              *result->economic_facilities, result->identity);
        if (result->governance_work_fees)
          result->governance_work_fees = copy_locked_native_catalog(
              *result->governance_work_fees, result->identity);
        auto facility_catalogs =
            validate_native_economic_facility_catalogs(*result);
        if (!facility_catalogs.ok)
          return {false, {}, std::move(facility_catalogs.error)};
        if (result->organization_lifecycle) {
          result->organization = copy_locked_native_catalog(
              *result->organization, result->identity);
          result->organization_lifecycle = copy_locked_native_catalog(
              *result->organization_lifecycle, result->identity);
        }
        if (result->governance_program_catalog)
          result->governance_program_catalog = copy_locked_native_catalog(
              *result->governance_program_catalog, result->identity);
        if (result->dependent_provision_catalog)
          result->dependent_provision_catalog = copy_locked_native_catalog(
              *result->dependent_provision_catalog, result->identity);
        auto organization_selected =
            validate_native_organization_runtime_catalog(*result);
        if (!organization_selected.ok)
          return {false, {}, std::move(organization_selected.error)};
        auto obligation_selected =
            validate_native_economic_obligation_catalog(*result);
        if (!obligation_selected.ok)
          return {false, {}, std::move(obligation_selected.error)};
        auto governance_selected =
            validate_native_governance_program_catalog(*result);
        if (!governance_selected.ok)
          return {false, {}, std::move(governance_selected.error)};
        auto birth_selected = validate_native_family_birth_catalog(*result);
        if (!birth_selected.ok)
          return {false, {}, std::move(birth_selected.error)};
        auto population_selected =
            validate_native_born_population_catalog(*result);
        if (!population_selected.ok)
          return {false, {}, std::move(population_selected.error)};
        if (result->social && result->social->original_relations)
          result->social =
              copy_locked_native_catalog(*result->social, result->identity);
        if (result->actor_foundation) {
          auto locked_foundation = copy_locked_native_catalog(
              *result->actor_foundation, result->identity);
          result->actor_foundation =
              seal_actor_foundation_catalog(std::move(*locked_foundation));
        }
        if (result->actor_social_occupancy_initial)
          result->actor_social_occupancy_initial = copy_locked_native_catalog(
              *result->actor_social_occupancy_initial, result->identity);
        if (result->growth_permanent)
          result->growth_permanent = copy_locked_native_catalog(
              *result->growth_permanent, result->identity);
        auto shop_catalog =
            validate_native_private_shop_operation_catalog(*result);
        if (!shop_catalog.ok)
          return {false, {}, std::move(shop_catalog.error)};
        auto task_execution_catalog =
            validate_native_task_execution_catalog(*result);
        if (!task_execution_catalog.ok)
          return {false, {}, std::move(task_execution_catalog.error)};
        auto work_catalog = validate_native_task_work_catalog(*result);
        if (!work_catalog.ok)
          return {false, {}, std::move(work_catalog.error)};
        auto permanent = validate_native_growth_permanent_catalog(*result);
        if (!permanent.ok)
          return {false, {}, std::move(permanent.error)};
        auto item_craft_selected = validate_native_item_craft_catalogs(*result);
        if (!item_craft_selected.ok)
          return {false, {}, std::move(item_craft_selected.error)};
        auto military_selected =
            validate_native_military_state_catalog(*result);
        if (!military_selected.ok)
          return {false, {}, std::move(military_selected.error)};
        auto personal_lifecycle_selected =
            validate_native_personal_battle_lifecycle_catalog(*result);
        if (!personal_lifecycle_selected.ok)
          return {false, {}, std::move(personal_lifecycle_selected.error)};
        auto battle_lifecycle_selected =
            validate_native_military_battle_lifecycle_catalog(*result);
        if (!battle_lifecycle_selected.ok)
          return {false, {}, std::move(battle_lifecycle_selected.error)};
        auto control = validate_native_control_continuation_catalog(*result);
        if (!control.ok)
          return {false, {}, std::move(control.error)};
        auto life_age = validate_native_life_age_catalog(*result);
        if (!life_age.ok)
          return {false, {}, std::move(life_age.error)};
        auto social_selection =
            validate_native_social_catalog_selection(*result);
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
        auto interaction_selection =
            validate_native_interaction_catalog(*result);
        if (!interaction_selection.ok)
          return {false, {}, std::move(interaction_selection.error)};
        auto transport_selection = validate_native_transport_catalog(*result);
        if (!transport_selection.ok)
          return {false, {}, std::move(transport_selection.error)};
        auto authored_boundary_flags = prepare_native_movement_boundary_flags(
            field(catalog, "data", "catalog"), *result);
        if (!authored_boundary_flags.ok)
          return {false, {}, std::move(authored_boundary_flags.error)};
        auto boundary_selection =
            validate_native_movement_boundary_catalog(*result);
        if (!boundary_selection.ok)
          return {false, {}, std::move(boundary_selection.error)};
        auto continuation_selection =
            selected_native_movement_boundary_continuation_version(result->identity);
        if (!continuation_selection.ok)
          return {false, {}, std::move(continuation_selection.error)};
        auto local_move_selection =
            selected_native_local_move_version(result->identity);
        if (!local_move_selection.ok)
          return {false, {}, std::move(local_move_selection.error)};
        auto handoff_selection =
            validate_native_group_member_handoff_catalog(*result);
        if (!handoff_selection.ok)
          return {false, {}, std::move(handoff_selection.error)};
        auto director_selection =
            validate_native_director_catalog_selection(*result);
        if (!director_selection.ok)
          return {false, {}, std::move(director_selection.error)};
        // 从原 data 重建最终 admitted 的选择，不继承 effective 中间目录锁。
        auto care_selection = prepare_native_care_root_capability_selection(
            field(catalog, "data", "catalog"), scenario_id, *result);
        if (!care_selection.ok)
          return {false, {}, std::move(care_selection.error)};
        auto relations_catalog =
            prepare_native_political_relations_catalog(scenario, *result);
        if (!relations_catalog.ok)
          return {false, {}, std::move(relations_catalog.error)};
        auto field_passage_catalog = prepare_native_field_passage_catalog(scenario, *result);
        if (!field_passage_catalog.ok)
          return {false, {}, std::move(field_passage_catalog.error)};
        return {true, std::move(result), {}};
      });
}
// 全部域在本地候选中初始化，任何拒绝都丢弃本次已分配的初货ID与对象。
Outcome<NativeInitialWorld>
make_native_initial_world(const BoundaryValue &catalog,
                          const NativeInitRequest &request,
                          const FieldBattleResourceRead *resources) {
  try {
    using namespace boundary;
    if (request.scenario_id.empty() || request.world_seed.empty() ||
        request.world_seed.size() > 256)
      fail("InvalidNativeInitRequest", "request");
    const auto &data = field(catalog, "data", "catalog");
    const auto &scenario = field(field(data, "scenarios", "data"),
                                 request.scenario_id, "scenarios");
    const auto &dynamic = field(scenario, "dynamic_world", request.scenario_id);
    if (get_string(dynamic, "profile", request.scenario_id) !=
        "local_society_v1")
      unsupported("UnsupportedNativeProfile", request.scenario_id);
    const auto policy =
        get_string(dynamic, "founder_policy", request.scenario_id);
    if (policy != "fixed_actor" && policy != "fixed_actor_appearance_v1" &&
        policy != "fixed_actor_appearance_composition_v1" &&
        !native_existing_actor_policy(policy) &&
        policy != "created_character_v1")
      unsupported("UnsupportedFounderPolicy", request.scenario_id);
    const bool creating = policy == "created_character_v1";
    if (creating) {
      if (!request.character_creation_request || request.founder_selection ||
          request.appearance_request || request.unlocked_actor_ids)
        fail("CharacterCreationRequestRequired", "request");
      fields(*request.character_creation_request,
             {"draft", "appearance_choice_id"}, {},
             "request/character_creation_request");
    } else if (request.character_creation_request)
      fail("CharacterCreationCapabilityRequired",
           "request/character_creation_request");
    auto player = get_string(scenario, "player_actor", request.scenario_id);
    if (policy == "fixed_actor" && request.appearance_request &&
        (!request.appearance_request->as<V::Dict>() ||
         !request.appearance_request->as<V::Dict>()->empty()))
      unsupported("FixedActorAppearanceRequired", "request/appearance_request");
    if (!native_existing_actor_policy(policy) && request.founder_selection &&
        *request.founder_selection != player)
      fail("InvalidPlayableActor", "request/founder_selection");
    const auto playable =
        strings(field(scenario, "playable_actor_ids", request.scenario_id),
                request.scenario_id);
    if (!creating && !native_existing_actor_policy(policy) &&
        (playable.size() != 1 || playable.front() != player))
      unsupported("FixedActorSelectionRequired", request.scenario_id);
    auto prepared = prepare_native_catalog_for_install(
        catalog, request.scenario_id, request.expected_catalog_identity,
        resources);
    if (!prepared.ok)
      return {false, {}, std::move(prepared.error)};
    auto effective = effective_catalog(catalog, request);
    const auto &current_data = field(effective, "data", "catalog");
    const auto &initial = field(field(field(current_data, "scenarios", "data"),
                                      request.scenario_id, "scenarios"),
                                "initial", request.scenario_id);
    if (native_existing_actor_policy(policy)) {
      auto actor = select_native_actor(current_data, scenario,
                                       request.founder_selection);
      if (!actor.ok)
        return {false, {}, actor.error};
      player = actor.value;
    }
    if (prepared.value->growth->actor_start.cards.enabled) {
      const auto allowed = validate_growth_actor_start_selection(
          prepared.value->growth->actor_start, player,
          request.unlocked_actor_ids.value_or(std::vector<StableId>{}));
      if (!allowed.ok)
        return {false, {}, allowed.error};
    } else if (request.unlocked_actor_ids)
      fail("ActorCardsCapabilityRequired", "request/unlocked_actor_ids");
    if (creating && !playable.empty())
      fail("CharacterCreationExistingActorConflict", "playable_actor_ids");
    auto selected_appearance =
        creating
            ? Outcome<std::shared_ptr<const AppearanceProfile>>{true, {}, {}}
        : native_existing_actor_policy(policy)
            ? select_native_appearance(current_data, scenario,
                                       request.appearance_request, player)
            : select_native_appearance(current_data, scenario,
                                       request.appearance_request);
    if (!selected_appearance.ok)
      return {false, {}, selected_appearance.error};
    auto mutable_catalog = prepared.value;
    NativeInitialWorld result;
    result.catalog = mutable_catalog;
    auto &seed = result.seed;
    auto &common = seed.common;
    // 初装、原组织和其它seed证据必须从开始就借同一真WorldId；安装不回写旧证据。
    common.identity.world_id = allocate_native_world_id();
    common.metadata = {mutable_catalog->identity.ruleset_id,
                       mutable_catalog->identity.content_id,
                       mutable_catalog->identity.content_version,
                       mutable_catalog->identity.content_digest,
                       "4.7.2-stable (official)",
                       "godot:4.7.2-stable (official)",
                       request.scenario_id,
                       player};
    seed_rng(common, request.world_seed);
    auto travel = checkpoint_make_shared<TravelState>();
    auto interaction = checkpoint_make_shared<InteractionState>();
    if (mutable_catalog->known_personal_dialogues)
      interaction->known_personal_dialogues.emplace();
    if (mutable_catalog->born_personal_dialogues)
      interaction->born_personal_dialogues.emplace();
    if (mutable_catalog->director && mutable_catalog->director->visit_reception)
      interaction->visit_reception_claims.emplace();
    auto interaction_roots =
        initialize_native_interaction_roots(*mutable_catalog, *interaction);
    if (!interaction_roots.ok)
      return {false, {}, std::move(interaction_roots.error)};
    auto face_roots =
        initialize_native_face_to_face_sources(*mutable_catalog, *interaction);
    if (!face_roots.ok)
      return {false, {}, std::move(face_roots.error)};
    auto seeded =
        seed_native_actors(common, current_data, initial, *travel, *interaction,
                           mutable_catalog->common->extended_skills,
                           mutable_catalog->actor_foundation.get());
    if (!seeded.ok)
      return {false, {}, seeded.error};
    if (mutable_catalog->actor_social_occupancy_selected) {
      if (!mutable_catalog->actor_social_occupancy_initial)
        return {
            false,
            {},
            common_failure("ActorSocialOccupancyCompiledSeedRequired").error};
      auto actual = compile_actor_social_occupancy_initial(
          common, *interaction, mutable_catalog->identity, request.scenario_id);
      if (!actual.ok)
        return {false, {}, std::move(actual.error)};
      const auto &compiled =
          mutable_catalog->actor_social_occupancy_initial->authored_initial;
      if (compiled.order() != actual.value.authored_initial.order())
        return {
            false,
            {},
            common_failure("ActorSocialOccupancyOriginalSeedMismatch").error};
      for (const auto &row : actual.value.authored_initial.rows()) {
        const auto *expected = compiled.find(row.first);
        if (!expected ||
            expected->interaction_id != row.second.interaction_id ||
            expected->party_id != row.second.party_id)
          return {false,
                  {},
                  common_failure("ActorSocialOccupancyOriginalSeedMismatch",
                                 row.first)
                      .error};
      }
    }
    if (selected_appearance.value) {
      auto *selected = common.actors.find(player);
      if (!selected)
        fail("InvalidPlayableActor", player);
      selected->appearance = selected_appearance.value;
    }
    if (native_existing_actor_policy(policy)) {
      auto controlled = prepare_native_player_control(common, *travel, player);
      if (!controlled.ok)
        return {false, {}, controlled.error};
    } else if (!creating) {
      const auto *founder = common.actors.find(player);
      if (!founder || !founder->alive || !founder->available ||
          travel->itineraries.find(player))
        fail("InvalidPlayableActor", "request/founder_selection");
    }
    if (mutable_catalog->regional_life) {
      auto configured = initialize_regional_life(
          *interaction,
          !mutable_catalog->interaction->regional_outcome_topics.empty());
      if (!configured.ok)
        return {false, {}, configured.error};
    }
    seed.travel = travel;
    seed.interaction = interaction;
    seed.military = seed_military(*mutable_catalog->military, 0);
    if (mutable_catalog->naval) {
      auto n = checkpoint_make_shared<NavalState>();
      static_cast<NavalAssetState &>(*n) =
          initial_naval_assets(*mutable_catalog->naval);
      seed.naval = std::move(n);
    }
    seed.estate = seed_estates(common, *mutable_catalog->estate);
    auto social = checkpoint_make_shared<SocialState>();
    auto original_social_version =
        selected_native_original_social_version(mutable_catalog->identity);
    if (!original_social_version.ok)
      return {false, {}, std::move(original_social_version.error)};
    social->original_relation_version = original_social_version.value;
    for (const auto &row :
         object(field(current_data, "relations", "data"), "relations")) {
      const auto &v = row.second;
      RelationRecord relation;
      relation.id = row.first;
      relation.from_actor = get_string(v, "from_actor", row.first);
      relation.to_actor = get_string(v, "to_actor", row.first);
      relation.affinity = get_int(v, "affinity", row.first);
      relation.trust = get_int(v, "trust", row.first);
      social->relations.insert(row.first, std::move(relation));
    }
    auto dynamic_social =
        initialize_dynamic_social(*social, mutable_catalog->regional_life);
    if (!dynamic_social.ok)
      return {false, {}, dynamic_social.error};
    seed.social = social;
    if (mutable_catalog->family_life)
      seed.family_life = checkpoint_make_shared<const FamilyDeathState>();
    if (mutable_catalog->family_legacy)
      seed.family_legacy = checkpoint_make_shared<const FamilyLegacyState>();
    if (mutable_catalog->family_bequest)
      seed.family_bequest = checkpoint_make_shared<const FamilyBequestState>();
    if (mutable_catalog->family_inheritance)
      seed.family_inheritance =
          checkpoint_make_shared<const FamilyInheritanceState>();
    if (mutable_catalog->family_marriage)
      seed.family_marriage =
          checkpoint_make_shared<const FamilyMarriageState>();
    if (mutable_catalog->family_birth)
      seed.family_birth = checkpoint_make_shared<const FamilyBirthState>();
    if (mutable_catalog->family_care_relief)
      seed.child_care_relief =
          checkpoint_make_shared<const ChildCareReliefState>();
    if (mutable_catalog->personal_recovery)
      seed.personal_recovery =
          checkpoint_make_shared<const GrowthRecoveryState>();
    if (mutable_catalog->growth_tactic_lessons)
      seed.growth_tactic_lessons =
          checkpoint_make_shared<const GrowthTacticLessonState>();
    if (mutable_catalog->family_apprenticeship)
      seed.family_apprenticeship =
          checkpoint_make_shared<const FamilyApprenticeshipState>();
    if (mutable_catalog->family_disciple_legacy)
      seed.family_disciple_legacy =
          checkpoint_make_shared<const DiscipleControlState>();
    if (mutable_catalog->family_biological_legacy)
      seed.family_biological_legacy =
          checkpoint_make_shared<const BiologicalControlState>();
    if (mutable_catalog->family_collateral_legacy)
      seed.family_collateral_legacy =
          checkpoint_make_shared<const CollateralControlState>();
    auto family = checkpoint_make_shared<FamilyState>();
    family->founder_actor_id = player;
    seed.family = family;
    auto tasks = checkpoint_make_shared<TaskState>();
    if (mutable_catalog->task_offer_responses) {
      tasks->offer_responses.emplace();
      tasks->offer_responses->version =
          mutable_catalog->task_offer_response_version;
      if (mutable_catalog->task_offer_response_version == 2)
        tasks->offer_responses->runtime.emplace();
    }
    if (mutable_catalog->task_personal_advances)
      tasks->personal_advances.emplace();
    if (mutable_catalog->task_message_notices)
      tasks->message_notices.emplace();
    if (mutable_catalog->task_static_assets_enabled)
      tasks->static_assets.emplace();
    if (mutable_catalog->task_extensions)
      tasks->deadline_extensions.emplace();
    if (mutable_catalog->task_message_returns)
      tasks->message_returns.emplace();
    if (mutable_catalog->task_personal_messages)
      tasks->personal_messages.emplace();
    if (mutable_catalog->task_personal_reports)
      tasks->personal_reports.emplace();
    if (mutable_catalog->task_personal_acceptances)
      tasks->personal_acceptances.emplace();
    if (mutable_catalog->task_personal_cancellations)
      tasks->personal_cancellations.emplace();
    if (mutable_catalog->task_message_handoffs_enabled) {
      tasks->message_handoffs.emplace();
      tasks->message_handoffs->version =
          mutable_catalog->task_message_handoff_version;
    }
    if (mutable_catalog->task_takeover_execution_enabled) {
      tasks->takeover_execution.emplace();
      tasks->takeover_execution->version =
          mutable_catalog->political_takeover->task_execution_version;
    }
    auto work_root = initialize_native_task_work_root(*mutable_catalog, *tasks);
    if (!work_root.ok)
      return {false, {}, std::move(work_root.error)};
    seed.tasks = tasks;
    if (mutable_catalog->envoy_tasks_enabled) {
      auto envoy = checkpoint_make_shared<EnvoyTaskState>();
      envoy->version = mutable_catalog->diplomacy->version;
      seed.envoy_tasks = std::move(envoy);
    }
    if (mutable_catalog->governance_tasks_enabled)
      seed.governance_tasks =
          checkpoint_make_shared<const GovernanceTaskState>();
    seed.personal = checkpoint_make_shared<PersonalState>();
    std::shared_ptr<AuthoredActorFoundationState> exact_foundation;
    if (mutable_catalog->exact_time) {
      if (!mutable_catalog->actor_foundation)
        return {
            false, {}, common_failure("ExactActorFoundationRequired").error};
      auto authored = make_authored_actor_foundation_seed(
          common, *mutable_catalog->actor_foundation);
      if (!authored.ok)
        return {false, {}, authored.error};
      exact_foundation = checkpoint_make_shared<AuthoredActorFoundationState>(
          std::move(authored.value));
    }
    if (creating) {
      auto draft = decode_character_creation_draft(
          field(*request.character_creation_request, "draft",
                "request/character_creation_request"),
          mutable_catalog->common->extended_skills);
      if (!draft.ok)
        return {false, {}, draft.error};
      CommonStore creation_common(std::move(common), mutable_catalog->common);
      const auto appearance = get_string(*request.character_creation_request,
                                         "appearance_choice_id",
                                         "request/character_creation_request");
      Outcome<StableId> made;
      if (mutable_catalog->exact_time &&
          exact_complete_selected(*mutable_catalog->exact_time)) {
        auto calendar = OriginalCalendarRead::admit(
            mutable_catalog->identity,
            {original_calendar_rule_id, 1,
             mutable_catalog->identity.content_digest});
        if (!calendar.ok)
          return {false, {}, calendar.error};
        NativeInitialMomentRead clock(creation_common);
        CharacterCreationV2Write created{*exact_foundation,
                                         *mutable_catalog->actor_foundation,
                                         calendar.value, clock};
        made = initialize_character_creation_v2(
            creation_common, *family, *interaction, *social,
            *mutable_catalog->family, *mutable_catalog->common, draft.value,
            appearance, created);
      } else
        made = initialize_character_creation(
            creation_common, *family, *interaction, *social,
            *mutable_catalog->family, *mutable_catalog->common, draft.value,
            appearance);
      if (!made.ok)
        return {false, {}, made.error};
      common = std::move(creation_common.state());
      player = made.value;
    }
    auto growth = checkpoint_make_shared<GrowthState>();
    auto card_initial =
        initialize_growth_card_state(common, *growth, *mutable_catalog->growth);
    if (!card_initial.ok)
      return {false, {}, card_initial.error};
    seed.growth = growth;
    CommonStore initial_common(std::move(common), mutable_catalog->common);
    auto task_execution_root = initialize_native_task_execution_root(
        initial_common, *mutable_catalog, *tasks);
    if (!task_execution_root.ok)
      return {false, {}, std::move(task_execution_root.error)};
    auto economy = make_initial_local_economy(
        initial_common, *mutable_catalog->local_economy, 0);
    if (!economy.ok)
      return {false, {}, economy.error};
    seed.local_economy = checkpoint_make_shared<const LocalEconomyState>(
        std::move(economy.value));
    auto obligation_root =
        initialize_native_economic_obligation_root(*mutable_catalog, seed);
    if (!obligation_root.ok)
      return {false, {}, std::move(obligation_root.error)};
    if (mutable_catalog->economic_supply_policies_enabled) {
      auto state = checkpoint_make_shared<EconomicSupplyPolicyState>();
      state->initialized_tick = initial_common.tick();
      state->last_observation_tick = initial_common.tick();
      seed.economic_supply_policies = std::move(state);
      seed.political_supply_adoptions =
          checkpoint_make_shared<const PoliticalSupplyAdoptionState>();
    }
    if (mutable_catalog->estate_employment) {
      auto employment = make_initial_estate_employment(
          initial_common, *seed.estate, *seed.local_economy);
      if (!employment.ok)
        return {false, {}, employment.error};
      seed.estate_employment =
          checkpoint_make_shared<const EstateEmploymentState>(
              std::move(employment.value));
    }
    if (mutable_catalog->military->battle_catalog.tactic_learning) {
      auto learned = checkpoint_make_shared<MilitaryTacticLearningState>();
      auto ready = initialize_military_tactic_learning_in_candidate(
          initial_common, *mutable_catalog->common,
          *mutable_catalog->military->battle_catalog.tactic_learning, *learned);
      if (!ready.ok)
        return {false, {}, ready.error};
      seed.military_tactic_learning = std::move(learned);
    }
    if (mutable_catalog->family->household_control) {
      Status control;
      if (mutable_catalog->exact_time &&
          exact_complete_selected(*mutable_catalog->exact_time)) {
        if (!exact_foundation || !mutable_catalog->actor_foundation)
          return {
              false,
              {},
              common_failure("CompleteFamilyOriginalSourcesRequired").error};
        NativeInitialMomentRead clock(initial_common);
        auto calendar = OriginalCalendarRead::admit(
            mutable_catalog->identity,
            {original_calendar_rule_id, 1,
             mutable_catalog->identity.content_digest});
        if (!calendar.ok)
          return {false, {}, std::move(calendar.error)};
        ProfileControlEligibilitySources eligibility{
            initial_common,
            *family,
            *mutable_catalog->family,
            *mutable_catalog->common,
            seed.family_legacy.get(),
            exact_foundation.get(),
            mutable_catalog->actor_foundation.get(),
            &clock,
            seed.control_continuation.get()};
        const FamilyBirthState unselected_births;
        LifeAgeSources initial_age{initial_common,
                                   *exact_foundation,
                                   *mutable_catalog->actor_foundation,
                                   *family,
                                   seed.family_birth ? *seed.family_birth
                                                     : unselected_births,
                                   eligibility,
                                   calendar.value};
        control = initialize_household_control(
            initial_common, *family, *mutable_catalog->common,
            *seed.local_economy, *mutable_catalog->family->household_control,
            &initial_age);
      } else {
        control = initialize_household_control(
            initial_common, *family, *mutable_catalog->common,
            *seed.local_economy, *mutable_catalog->family->household_control);
      }
      if (!control.ok)
        return {false, {}, std::move(control.error)};
    }
    if (mutable_catalog->household_provision_management) {
      auto checked = validate_household_provision_management_catalog(
          initial_common, *mutable_catalog->local_economy,
          *mutable_catalog->household_provision_management);
      if (!checked.ok)
        return {false, {}, checked.error};
      seed.household_provision_management =
          checkpoint_make_shared<const HouseholdProvisionManagementState>();
    }
    if (mutable_catalog->production_disruptions) {
      auto created = make_initial_production_disruptions_in_candidate(
          initial_common, *mutable_catalog->local_economy,
          *mutable_catalog->production_disruptions);
      if (!created.ok)
        return {false, {}, created.error};
      seed.production_disruptions =
          checkpoint_make_shared<const ProductionDisruptionState>(
              std::move(created.value));
    }
    // 可写别名仅限当前未发布 initializer；外部仍只看到最终完整 seed。
    std::shared_ptr<OrganizationState> deferred_authored_organization;
    std::shared_ptr<OrganizationRuntime> deferred_authored_runtime;
    auto organization_runtime =
        selected_native_organization_runtime(mutable_catalog->identity);
    if (!organization_runtime.ok)
      return {false, {}, std::move(organization_runtime.error)};
    if (organization_runtime.value) {
      if (!seed.economic_obligation_sources ||
          !mutable_catalog->organization_lifecycle)
        return {
            false,
            {},
            common_failure("OrganizationRuntimeActualObligationRootRequired")
                .error};
      auto organization = checkpoint_make_shared<OrganizationState>();
      auto runtime = checkpoint_make_shared<OrganizationRuntime>();
      bool needs_installed_origin = false;
      // 原目录准入已核各 body 与其唯一 seed policy；未被引用的 policy
      // 不触发新顺序。
      for (const auto &row :
           mutable_catalog->organization->organizations.rows())
        needs_installed_origin =
            needs_installed_origin ||
            row.second.status == OrganizationStatus::active;
      if (needs_installed_origin) {
        // 唯一原身体安装；此时不分配 lifecycle origin，也不宣布 active 就绪。
        auto initial = make_initial_organization(
            initial_common, *mutable_catalog->organization);
        if (!initial.ok)
          return {false, {}, std::move(initial.error)};
        *organization = std::move(initial.value);
        deferred_authored_organization = organization;
        deferred_authored_runtime = runtime;
      } else {
        auto initialized =
            initialize_authored_organization_runtime_in_candidate(
                initial_common, *organization, *runtime,
                *mutable_catalog->organization,
                *mutable_catalog->organization_lifecycle,
                *mutable_catalog->local_economy, *seed.local_economy,
                *seed.economic_obligation_sources);
        if (!initialized.ok)
          return {false, {}, std::move(initialized.error)};
      }
      seed.organization = std::move(organization);
      seed.organization_runtime = std::move(runtime);
    } else {
      auto organization = make_initial_organization(
          initial_common, *mutable_catalog->organization);
      if (!organization.ok)
        return {false, {}, organization.error};
      seed.organization = checkpoint_make_shared<const OrganizationState>(
          std::move(organization.value));
    }
    {
      auto initialized =
          initialize_native_governance_program_root(*mutable_catalog, seed);
      if (!initialized.ok)
        return {false, {}, std::move(initialized.error)};
    }
    if (mutable_catalog->politics && mutable_catalog->politics->enabled) {
      auto politics = make_initial_politics(
          initial_common, *mutable_catalog->common, *mutable_catalog->politics,
          *seed.organization, *mutable_catalog->organization);
      if (!politics.ok)
        return {false, {}, politics.error};
      seed.politics = checkpoint_make_shared<const PoliticsState>(
          std::move(politics.value));
    }
    if (mutable_catalog->envoy_tasks_enabled) {
      auto diplomacy = checkpoint_make_shared<DiplomacyState>();
      diplomacy->version = mutable_catalog->diplomacy->version;
      if (diplomacy->version == 2)
        diplomacy->v2.emplace();
      seed.diplomacy = std::move(diplomacy);
    }
    if (mutable_catalog->treaty_passage &&
        mutable_catalog->treaty_passage->enabled)
      seed.treaty_passage = checkpoint_make_shared<const TreatyPassageState>();
    if (mutable_catalog->political_military_appointments &&
        mutable_catalog->political_military_appointments->enabled)
      seed.political_military_appointments =
          checkpoint_make_shared<const PoliticalMilitaryAppointmentState>();
    if (mutable_catalog->political_field_appointments &&
        mutable_catalog->political_field_appointments->enabled)
      seed.political_field_appointments =
          checkpoint_make_shared<const PoliticalFieldAppointmentState>();
    if (mutable_catalog->occupation_fiscal &&
        mutable_catalog->occupation_fiscal->enabled) {
      seed.occupation_fiscal =
          checkpoint_make_shared<const OccupationFiscalState>();
      seed.occupation_fiscal_settlement =
          checkpoint_make_shared<const OccupationFiscalSettlementState>();
    }
    if (mutable_catalog->political_takeover &&
        mutable_catalog->political_takeover->enabled) {
      auto takeover = checkpoint_make_shared<PoliticalTakeoverState>();
      takeover->version = mutable_catalog->political_takeover->version;
      seed.political_takeover = std::move(takeover);
      seed.economic_takeover_execution =
          checkpoint_make_shared<const EconomicTakeoverExecutionState>();
    }
    if (mutable_catalog->political_fiscal &&
        mutable_catalog->political_fiscal->enabled) {
      if (mutable_catalog->political_fiscal->version == 2) {
        auto fiscal =
            initialize_political_fiscal2(*mutable_catalog->political_fiscal);
        if (!fiscal.ok)
          return {false, {}, std::move(fiscal.error)};
        seed.political_fiscal =
            checkpoint_make_shared<const PoliticalFiscalState>(
                std::move(fiscal.value));
      } else {
        seed.political_fiscal =
            checkpoint_make_shared<const PoliticalFiscalState>();
      }
    }
    if (mutable_catalog->fiscal_settlement) {
      if (!seed.politics || !seed.political_fiscal)
        return {
            false,
            {},
            common_failure("FiscalSettlementRequiresPoliticalFiscal").error};
      OrganizationAccess organizations(initial_common, *seed.organization,
                                       *mutable_catalog->organization,
                                       *seed.local_economy);
      FiscalPolicyAccess policies(initial_common, *mutable_catalog->common,
                                  *seed.politics, *seed.political_fiscal,
                                  *mutable_catalog->political_fiscal);
      auto fiscal = make_initial_fiscal_settlement(
          initial_common, *seed.local_economy, *mutable_catalog->local_economy,
          organizations, policies, *mutable_catalog->fiscal_settlement);
      if (!fiscal.ok)
        return {false, {}, fiscal.error};
      seed.fiscal_settlement =
          checkpoint_make_shared<const FiscalSettlementState>(
              std::move(fiscal.value));
    }
    if (mutable_catalog->fiscal_taxpayer_continuity_enabled) {
      if (!seed.fiscal_settlement)
        return {false,
                {},
                common_failure("FiscalTaxpayerContinuityDependenciesRequired")
                    .error};
      auto initial = make_initial_fiscal_taxpayer_continuity(
          initial_common, *seed.fiscal_settlement);
      if (!initial.ok)
        return {false, {}, initial.error};
      seed.fiscal_taxpayer_continuity =
          checkpoint_make_shared<const FiscalTaxpayerContinuityState>(
              std::move(initial.value));
    }
    if (mutable_catalog->governance_tasks_enabled) {
      if (!seed.politics || !mutable_catalog->politics_governance)
        return {false, {}, common_failure("IncompleteGovernanceProfile").error};
      auto governance = checkpoint_make_shared<PoliticsGovernanceState>();
      auto valid = initialize_political_civil_state_in_transaction(
          initial_common, *seed.politics, *governance,
          *mutable_catalog->politics_governance);
      if (!valid.ok)
        return {false, {}, valid.error};
      seed.politics_governance = std::move(governance);
    }
    if (mutable_catalog->public_relief)
      seed.public_relief = checkpoint_make_shared<const PublicReliefState>();
    if (mutable_catalog->political_governance_continuations)
      seed.political_governance_continuations =
          checkpoint_make_shared<const PoliticalGovernanceContinuationState>();
    if (mutable_catalog->political_governance_invitation_replies)
      seed.governance_invitation_replies =
          checkpoint_make_shared<const GovernanceInvitationReplyState>();
    if (mutable_catalog->political_head_handovers)
      seed.political_head_handovers =
          checkpoint_make_shared<const PoliticalHeadHandoverState>();
    if (mutable_catalog->political_successor_rounds)
      seed.political_successor_rounds =
          checkpoint_make_shared<const PoliticalSuccessorRoundState>();
    if (mutable_catalog->political_term_handovers)
      seed.political_term_handovers =
          checkpoint_make_shared<const PoliticalTermHandoverState>();
    if (mutable_catalog->political_office_receptions)
      seed.political_office_receptions =
          checkpoint_make_shared<const PoliticalOfficeReceptionState>();
    if (mutable_catalog->political_office_resignations) {
      auto state = checkpoint_make_shared<PoliticalOfficeResignationState>();
      state->version = mutable_catalog->political_office_resignations->version;
      seed.political_office_resignations = std::move(state);
    }
    if (mutable_catalog->political_subordinate_appointments) {
      auto state =
          checkpoint_make_shared<PoliticalSubordinateAppointmentState>();
      state->version =
          mutable_catalog->political_subordinate_appointments->version;
      seed.political_subordinate_appointments = std::move(state);
    }
    if (mutable_catalog->political_dismissal_sources) {
      auto state = checkpoint_make_shared<PoliticalDismissalSourceState>();
      state->version = mutable_catalog->political_dismissal_sources->version;
      seed.political_dismissal_sources = std::move(state);
    }
    if (mutable_catalog->political_governance_agencies) {
      auto state = checkpoint_make_shared<PoliticalGovernanceAgencyState>();
      state->version = mutable_catalog->political_governance_agencies->version;
      seed.political_governance_agencies = std::move(state);
    }
    if (mutable_catalog->family_voluntary_paths)
      seed.family_voluntary_paths =
          checkpoint_make_shared<const VoluntaryPathState>();
    if (mutable_catalog->political_successions)
      seed.political_successions =
          checkpoint_make_shared<const PoliticalSuccessionState>();
    auto affairs = make_initial_affairs(*mutable_catalog->affairs);
    if (!affairs.ok)
      return {false, {}, affairs.error};
    seed.affairs =
        checkpoint_make_shared<const AffairsState>(std::move(affairs.value));
    auto director =
        make_initial_director(initial_common, *mutable_catalog->director);
    if (!director.ok)
      return {false, {}, director.error};
    seed.director =
        checkpoint_make_shared<const DirectorState>(std::move(director.value));
    if (mutable_catalog->population_military &&
        mutable_catalog->population_military->enabled)
      seed.population_military =
          checkpoint_make_shared<const PopulationMilitaryState>();
    if (mutable_catalog->player_plans) {
      auto selected_plan =
          read_player_plan_selected_version(mutable_catalog->identity);
      if (!selected_plan.ok)
        return {false, {}, selected_plan.error};
      PlayerPlanState initial_plan;
      initial_plan.version = selected_plan.value;
      seed.player_plan = checkpoint_make_shared<const PlayerPlanState>(
          std::move(initial_plan));
    }
    std::shared_ptr<FacilityClaimsState> initial_facility_claims;
    if (mutable_catalog->exact_time) {
      // 仅作者冷入口生成真实初态；恢复不会走本段，不先写legacy生日/HP再回灌。
      if (!mutable_catalog->actor_foundation)
        return {
            false, {}, common_failure("ExactActorFoundationRequired").error};
      // This is the same owned initial source built before any dynamic
      // creation. A Created actor has already appended its actual
      // registration/seed here.
      if (!exact_foundation)
        return {
            false, {}, common_failure("ExactActorFoundationRequired").error};
      seed.actor_foundation = std::move(exact_foundation);
      auto injury_version = selected_native_character_injury_source_version(
          mutable_catalog->identity);
      if (!injury_version.ok)
        return {false, {}, std::move(injury_version.error)};
      if (injury_version.value == 1) {
        auto original = make_initial_character_injury_sources_v1(
            initial_common, *seed.actor_foundation,
            *mutable_catalog->actor_foundation);
        if (!original.ok)
          return {false, {}, std::move(original.error)};
        seed.character_injury_sources =
            checkpoint_make_shared<const CharacterInjurySourcesStateV1>(
                std::move(original.value));
      }

      if (mutable_catalog->growth->complete_capability_application_chain) {
        // 只在新局原初态中逐真 Origin 初始化；恢复不补造 revision 或旧卡映射。
        if (!mutable_catalog->growth_permanent)
          return {false,
                  {},
                  common_failure("GrowthPermanentCatalogRequired").error};
        auto permanent = checkpoint_make_shared<GrowthPermanentState>();
        NativeInitialMomentRead clock(initial_common);
        auto now = clock.read_current_moment();
        if (!now.ok)
          return {false, {}, std::move(now.error)};
        CharacterConditionSources body{initial_common, *seed.actor_foundation,
                                       *mutable_catalog->actor_foundation,
                                       std::move(now.value)};
        body.growth = growth.get();
        body.growth_catalog = mutable_catalog->growth.get();
        body.shared = mutable_catalog->common.get();
        body.family_catalog = mutable_catalog->family.get();
        body.permanent_growth = permanent.get();
        body.permanent_growth_catalog = mutable_catalog->growth_permanent.get();
        for (const auto &origin :
             seed.actor_foundation->character_origin_records.origins.rows()) {
          auto initialized =
              initialize_growth_capability_revision_for_actor_in_candidate(
                  initial_common, *permanent, body, origin.first);
          if (!initialized.ok)
            return {false, {}, std::move(initialized.error)};
        }
        seed.growth_permanent = std::move(permanent);
        // 最后两根由 fresh WorldBundle 中的原卡取得 writer 核实并安装，不能以空
        // seed 自证完成。
      }
      auto timeline = checkpoint_make_shared<WorldTimelineState>();
      timeline->scale_ref = mutable_catalog->exact_time->scale_ref;
      auto period = initial_common.next_id("world_period");
      if (!period.ok)
        return {false, {}, period.error};
      timeline->period_id = period.value;
      seed.world_timeline = std::move(timeline);
      seed.action_elapsed = checkpoint_make_shared<const ActionElapsedStore>();
      if (exact_social_selected(*mutable_catalog->exact_time) ||
          exact_complete_selected(*mutable_catalog->exact_time)) {
        if (!social->dynamic)
          return {false,
                  {},
                  common_failure("ExactSocialInitialOwnerRequired").error};
        auto social_version =
            selected_native_dynamic_social_version(*mutable_catalog);
        if (!social_version.ok)
          return {false, {}, std::move(social_version.error)};
        social->dynamic->version = social_version.value;
        social->dynamic->time2.emplace();
        social->dynamic->time2->collection_cursor.through_moment = {
            initial_common.tick(), {}};
        seed.social_period_sources =
            checkpoint_make_shared<const SocialPeriodSources>();
      }
      if (exact_complete_selected(*mutable_catalog->exact_time)) {
        initial_facility_claims = checkpoint_make_shared<FacilityClaimsState>();
        seed.facility_claims = initial_facility_claims;
      }
    }
    auto shop_selection = selected_native_private_shop_operation_versions(
        mutable_catalog->identity);
    if (!shop_selection.ok)
      return {false, {}, std::move(shop_selection.error)};
    if (mutable_catalog->private_shops &&
        (mutable_catalog->private_shops->version == 2 ||
         mutable_catalog->private_shops->version == 3) &&
        !seed.private_shops) {
      auto shops = make_initial_private_shops(
          initial_common, mutable_catalog->private_shops->version);
      if (!shops.ok)
        return {false, {}, std::move(shops.error)};
      seed.private_shops = checkpoint_make_shared<const PrivateShopState>(
          std::move(shops.value));
    }
    if (shop_selection.value.exact_operations()) {
      seed.private_shop_operation_sources =
          checkpoint_make_shared<const PrivateShopOperationSourcesState>();
      seed.private_shop_operation_capabilities =
          checkpoint_make_shared<const PrivateShopOperationCapabilitiesState>();
    }
    if (mutable_catalog->private_shop_household_custody)
      seed.private_shop_household_custody =
          checkpoint_make_shared<const PrivateShopHouseholdCustodyState>();
    auto item_craft_roots = initialize_native_item_craft_roots(
        initial_common, *mutable_catalog, seed);
    if (!item_craft_roots.ok)
      return {false, {}, std::move(item_craft_roots.error)};
    {
      NativeInitialMomentRead clock(initial_common);
      auto now = clock.read_current_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      auto facilities = initialize_native_economic_facility_roots(
          initial_common, *mutable_catalog, seed, initial_facility_claims.get(),
          now.value);
      if (!facilities.ok)
        return {false, {}, std::move(facilities.error)};
    }
    if (deferred_authored_organization) {
      if (!deferred_authored_runtime ||
          seed.organization.get() != deferred_authored_organization.get() ||
          seed.organization_runtime.get() != deferred_authored_runtime.get() ||
          !mutable_catalog->economic_facilities ||
          !seed.economic_facility_capacity_sources ||
          !mutable_catalog->local_economy->production_management ||
          !mutable_catalog->estate ||
          bool(mutable_catalog->private_shops) != bool(seed.private_shops))
        return {
            false,
            {},
            common_failure("OrganizationAuthoredInstalledOriginRootsRequired")
                .error};
      NativeInitialMomentRead clock(initial_common);
      auto now = clock.read_current_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      OrganizationAuthoredOriginContext original{
          initial_common,
          *mutable_catalog->organization,
          *mutable_catalog->organization_lifecycle,
          *mutable_catalog->local_economy,
          *seed.local_economy,
          *seed.economic_obligation_sources};
      EconomicFacilityInstallationReadContext installation{
          initial_common,
          *mutable_catalog->economic_facilities,
          *seed.economic_facility_capacity_sources,
          *mutable_catalog->local_economy->production_management,
          *mutable_catalog->estate,
          mutable_catalog->private_shops.get(),
          now.value};
      OrganizationAuthoredFixedOriginContext fixed_context{
          original, *mutable_catalog, installation};
      NativeOrganizationAuthoredOriginRead terminal(original);
      NativeOrganizationAuthoredFixedOriginRead fixed(fixed_context);
      NativeInitialOrganizationOriginRead sources(terminal, fixed);
      auto finished = finish_authored_organization_runtime_in_candidate(
          initial_common, *deferred_authored_organization,
          *deferred_authored_runtime, *mutable_catalog->organization,
          *mutable_catalog->organization_lifecycle,
          *mutable_catalog->local_economy, *seed.local_economy,
          *seed.economic_obligation_sources, sources, clock);
      if (!finished.ok)
        return {false, {}, std::move(finished.error)};
    }
    // Only after genuine installation may the original empty claim histories be
    // published.
    if (initial_facility_claims)
      for (const auto &slot : mutable_catalog->exact_time->complete
                                  ->facility_claims.selected_slots)
        initial_facility_claims->slot_histories.push_back(
            {slot, InitialEmptyFacilityClaims{}, {}});
    auto military_selection =
        selected_native_military_state_versions(mutable_catalog->identity);
    if (!military_selection.ok)
      return {false, {}, std::move(military_selection.error)};
    if (military_selection.value.dynamic_military)
      seed.military_exact = checkpoint_make_shared<const MilitaryExactState>();
    if (military_selection.value.military_action_time)
      seed.military_action_time =
          checkpoint_make_shared<const MilitaryActionTimeState>();
    if (mutable_catalog->field_battle)
      seed.field_battle = checkpoint_make_shared<const FieldBattleWorldState>();
    if (mutable_catalog->political_relations)
      seed.political_relations =
          checkpoint_make_shared<const PoliticalRelationsState>();
    if (mutable_catalog->field_group_passage) {
      seed.political_passage = checkpoint_make_shared<const PoliticalPassageDecisionState>();
      seed.political_safe_exit = checkpoint_make_shared<const SafeExitState>();
    }
    if (mutable_catalog->public_supply_inspection)
      seed.public_supply_inspection =
          checkpoint_make_shared<const PublicSupplyInspectionState>();
    if (mutable_catalog->control_continuation) {
      auto control = checkpoint_make_shared<ControlContinuationState>();
      auto initialized = initialize_control_continuation_in_candidate(
          initial_common, *control, *mutable_catalog->control_continuation,
          mutable_catalog->identity);
      if (!initialized.ok)
        return {false, {}, std::move(initialized.error)};
      seed.control_continuation = std::move(control);
    }
    if (mutable_catalog->life_age_checks) {
      if (!seed.actor_foundation || !seed.family_life ||
          !mutable_catalog->actor_foundation || !mutable_catalog->family)
        return {false,
                {},
                common_failure("LifeAgeOriginalSeedSourcesRequired", {},
                               ErrorKind::unsupported_capability)
                    .error};
      NativeInitialMomentRead clock(initial_common);
      auto calendar = OriginalCalendarRead::admit(
          mutable_catalog->identity,
          {original_calendar_rule_id, 1,
           mutable_catalog->identity.content_digest});
      if (!calendar.ok)
        return {false, {}, std::move(calendar.error)};
      ProfileControlEligibilitySources control{
          initial_common,
          *family,
          *mutable_catalog->family,
          *mutable_catalog->common,
          seed.family_legacy.get(),
          seed.actor_foundation.get(),
          mutable_catalog->actor_foundation.get(),
          &clock,
          seed.control_continuation.get()};
      const FamilyBirthState unselected_births;
      LifeAgeSources sources{initial_common,
                             *seed.actor_foundation,
                             *mutable_catalog->actor_foundation,
                             *family,
                             seed.family_birth ? *seed.family_birth
                                               : unselected_births,
                             control,
                             calendar.value};
      auto age = checkpoint_make_shared<LifeAgeState>();
      auto now = clock.read_current_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      auto initialized = initialize_life_age_state_from_original_seed(
          sources, *mutable_catalog->life_age_checks, *age, *seed.family_life,
          now.value);
      if (!initialized.ok)
        return {false, {}, std::move(initialized.error)};
      seed.life_age_checks = std::move(age);
    }
    if (mutable_catalog->actor_social_occupancy_selected) {
      if (!seed.actor_foundation || !mutable_catalog->actor_foundation ||
          !mutable_catalog->actor_social_occupancy_initial)
        return {false,
                {},
                common_failure("ActorSocialOccupancyOriginalSourcesRequired")
                    .error};
      NativeInitialMomentRead clock(initial_common);
      NativeActorSocialOccupancyInitialRead original(
          {initial_common, *mutable_catalog->common, *seed.actor_foundation,
           *mutable_catalog->actor_foundation, mutable_catalog->family.get(),
           *mutable_catalog->actor_social_occupancy_initial, clock});
      auto version = selected_native_actor_social_occupancy_version(
          mutable_catalog->identity);
      if (!version.ok)
        return {false, {}, std::move(version.error)};
      auto initialized = initialize_actor_social_occupancy_sources(
          initial_common, *interaction, original, clock, version.value);
      if (!initialized.ok)
        return {false, {}, std::move(initialized.error)};
    }
    auto transport_initial = initialize_native_transport_roots(
        *mutable_catalog, initial_common, seed);
    if (!transport_initial.ok)
      return {false, {}, std::move(transport_initial.error)};
    // 原P02/3逐cohort建立初始来源；身份和codec同值，空cohort也必须经原writer。
    auto population_version =
        selected_native_population_transport_version(mutable_catalog->identity);
    if (!population_version.ok)
      return {false, {}, std::move(population_version.error)};
    if (population_version.value) {
      if (seed.population_transport || !seed.local_economy)
        return {false,
                {},
                common_failure("PopulationTransportFreshOriginalRootsRequired")
                    .error};
      auto population = make_initial_population_transport(
          initial_common, *seed.local_economy, population_version.value);
      if (!population.ok)
        return {false, {}, std::move(population.error)};
      seed.population_transport =
          checkpoint_make_shared<const PopulationTransportState>(
              std::move(population.value));
    }
    if (mutable_catalog->production_cycles)
      seed.production_cycles =
          checkpoint_make_shared<const EconomicWorkSourcesState>();
    if (mutable_catalog->private_transport_funding_selected)
      seed.private_transport_funding =
          checkpoint_make_shared<const PrivateTransportFundingState>();
    if (mutable_catalog->household_purchases) {
      if (!mutable_catalog->household_retail_settlements ||
          (mutable_catalog->selected_household_version != 2 &&
           mutable_catalog->selected_household_version != 3))
        return {false,
                {},
                common_failure("HouseholdCatalogSelectionMismatch").error};
      auto purchases = checkpoint_make_shared<HouseholdPurchaseState>();
      auto adopted = adopt_locked_market_service_definitions_in_candidate(
          *purchases, *mutable_catalog->household_purchases, initial_common);
      if (!adopted.ok)
        return {false, {}, std::move(adopted.error)};
      seed.household_purchases = std::move(purchases);
      auto retail = checkpoint_make_shared<HouseholdRetailState>();
      retail->version = mutable_catalog->household_retail_settlements->version;
      seed.household_retail_settlements = std::move(retail);
    }
    common = std::move(initial_common.state());
    return {true, std::move(result), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace hanmo::resident
