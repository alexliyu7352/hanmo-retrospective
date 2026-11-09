#include "native_family_initial_sources.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/family/apprenticeship_views.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/military/military_supply_attrition_views.hpp"
#include "private_shop_liquidation_integration.hpp"
#include "resident/domains/interaction/known_contacts/known_contact_location_world.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/tasks/extensions/task_extension_world.hpp"
#include "resident/domains/military/military_recovery_views.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/party_consent/party_recruitment_consent.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff_profile.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
#include "political_takeover_execution_world.hpp"
#include "occupation_fiscal_integration.hpp"
#include "resident/domains/tasks/task_public_account_view.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset_view.hpp"
#include "resident/domains/family/adult_adoption.hpp"
#include "resident/domains/tasks/envoy_task_rules.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
#include "household_provision_integration.hpp"
#include "siege_integration.hpp"
// 已注册的当前玩家显示查询只组装同源typed叶结果；不寻路、不读全态、不创建第二候选。
#include "live_view_queries.hpp"
#include "political_governance_continuation_world.hpp"
#include "private_shop_integration.hpp"
#include "private_shop_daily_rent_integration.hpp"
#include "resident/domains/local_economy/economic_payload_guard.hpp"
#include "naval_integration.hpp"
#include "resident/domains/growth/growth_study_teaching_views.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "interaction_tasks_integration.hpp"
#include "runtime_task_response_integration.hpp"
#include "task_advance_integration.hpp"
#include "resident/domains/interaction/party_invitation_view.hpp"
#include "resident/domains/tasks/task_travel_manifest.hpp"
#include "resident/domains/tasks/task_route_obligations.hpp"
#include "economic_world_integration.hpp"
#include "resident/views/regional_life_view.hpp"
#include "resident/views/membership_view.hpp"
#include "resident/views/self_profile_view.hpp"
#include "resident/views/world_observation.hpp"
#include "resident/views/map_presence.hpp"
#include "resident/views/map_company.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/military_tactic_learning_views.hpp"
#include "treaty_passage_integration.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_missions.hpp"
#include "resident/domains/politics/politics_navigation.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/views/journey_motion.hpp"
#include "resident/views/journey_character.hpp"
#include "resident/views/common_presentation.hpp"
#include "resident/views/dynamic_world_view.hpp"
#include "resident/views/scene_presentation.hpp"
#include "resident/domains/military/military_views.hpp"
#include "resident/domains/military/camp_views.hpp"
#include "resident/domains/military/naval_views.hpp"
#include "resident/domains/military/naval_battle_views.hpp"
#include "resident/domains/military/siege_views.hpp"
#include "resident/domains/military/training_rules.hpp"
#include "resident/domains/military/military_codec.hpp"
namespace hanmo::resident {
namespace {
using V = BoundaryValue;
using D = V::Dict;
using A = V::Array;
V object(std::initializer_list<std::pair<std::string, V>> fields) {
  return V{D{fields}};
}
// 卡片的完整返报源仅借政治与任务的原权威读；页与摘要共用一次投影。
Outcome<GrowthActorCardsPage> actor_card_page(const WorldBundle &world,
                                              const StableId &actor,
                                              Integer offset, Integer limit) {
  const auto &cards = world.catalog->growth->actor_start.cards;
  const bool needs_envoy = std::any_of(
      cards.definitions.rows().begin(), cards.definitions.rows().end(),
      [](const auto &row) {
        return row.second.condition == "completed_envoy_round_trip_v1";
      });
  if (!needs_envoy)
    return project_growth_actor_cards(
        world.common.state(), *world.catalog->common, world.tasks,
        *world.catalog->tasks, cards, actor, offset, limit);
  if (cards.version != 2 || !world.envoy_tasks_enabled() ||
      world.envoy_tasks.version != 2 || !world.diplomacy.v2 ||
      !world.catalog->diplomacy || !world.catalog->organization)
    return {false, {}, common_failure("ActorCardEnvoySourceRequired").error};
  DiplomaticMissionAccess missions(world.common, world.organization,
                                   *world.catalog->organization,
                                   *world.catalog->diplomacy, world.diplomacy);
  DiplomaticReportAccess reports(
      DiplomaticHistoryRead{world.common, world.organization,
                            *world.catalog->organization,
                            *world.catalog->diplomacy, world.diplomacy},
      *world.diplomacy.v2);
  EnvoyTaskRead actual(world.common, world.tasks, world.envoy_tasks, missions,
                       missions, reports);
  return project_growth_actor_cards_with_envoy(
      world.common.state(), *world.catalog->common, world.tasks,
      *world.catalog->tasks, cards, world.envoy_tasks, actual, actor, offset,
      limit);
}
V point(const PointI &value) { return V{A{V{value.x}, V{value.y}}}; }
V destination(const TravelDestination &value) {
  if (auto id = std::get_if<StableId>(&value))
    return V{*id};
  return point(std::get<PointI>(value));
}
// 边界只读错误保留原码；真正未知能力不能被上层当普通空页继续。
Outcome<V> failure(const StableId &code, const StableId &query = {},
                   ErrorKind kind = ErrorKind::rule) {
  auto error = common_failure(code, {}, kind).error;
  error.phase = "query";
  error.domain = "presentation";
  error.command = query;
  return {false, {}, std::move(error)};
}
template <class T> Outcome<V> propagate(const Outcome<T> &result) {
  return {false, {}, result.error};
}
// 页字段顺序和可选next_cursor沿签收的presenter协议；不由边界重新切片。
template <class Page> void page_fields(D &fields, const Page &page) {
  fields.emplace_back("cursor", V{page.cursor});
  fields.emplace_back("total_rows", V{page.total_rows});
  if (page.next_cursor)
    fields.emplace_back("next_cursor", V{*page.next_cursor});
}
V location(const JourneyLocationSummary &value, bool sidebar) {
  D fields{{"kind", V{value.kind}}};
  if (value.place_id)
    fields.emplace_back("place_id", V{*value.place_id});
  if (sidebar && value.settlement_id)
    fields.emplace_back("settlement_id", V{*value.settlement_id});
  return V{std::move(fields)};
}
// 只有冷目录已证实当前place与所选map都无world模板，才允许旧地图画布；明确模板不伪装不存在。
Outcome<bool> world_scene_legacy(const WorldBundle &world,
                                 const StableId &player) {
  auto actor = world.common.actor(player);
  if (!actor)
    return {false, false, common_failure("UnknownSceneObserver").error};
  if (!actor->alive)
    return {true, false, {}};
  auto resolved = world.services.resolved_location(player);
  if (!resolved.ok)
    return {false, false, resolved.error};
  auto selection = WorldTemplateMapping::absent;
  if (auto current = std::get_if<SettlementLocation>(&resolved.value)) {
    auto place = world.catalog->common->places.find(current->place_id);
    if (!place || place->settlement_id != current->settlement_id)
      return {false, false,
              common_failure("InvalidSceneObserverLocation").error};
    bool known = false;
    for (auto &row : world.catalog->place_world_templates)
      if (row.first == current->place_id) {
        selection = row.second;
        known = true;
        break;
      }
    if (!known)
      return {false, false,
              failure("UnsupportedNativeCapability",
                      "world_scene_template_source",
                      ErrorKind::unsupported_capability)
                  .error};
  } else if (auto current = std::get_if<WildernessLocation>(&resolved.value)) {
    if (current->map_id != world.catalog->travel->world_map_id)
      return {false, false,
              common_failure("InvalidSceneObserverLocation").error};
  } else if (!std::holds_alternative<OnRouteLocation>(resolved.value))
    return {false, false,
            failure("UnsupportedNativeCapability", "world_scene_location",
                    ErrorKind::unsupported_capability)
                .error};
  if (selection == WorldTemplateMapping::absent)
    selection = world.catalog->world_template;
  // 路由只说明是否确实无模板；有模板或坏映射必须由实际scene_view给出投影/错误，不能阻塞无关城市页。
  return {true, selection == WorldTemplateMapping::absent, {}};
}
// HUD必须读本人私有钱包；在军团中也不能把公柜余额伪作个人钱数。
Outcome<V> hud(const WorldBundle &world, const V &args) {
  boundary::fields(args, {}, {"personal_battle_id"}, "args");
  const auto &common = world.common;
  const auto &player = common.state().metadata.player_actor_id;
  auto actor = common.actor(player);
  if (!actor || actor->id != player || actor->hp < 0 || common.tick() < 0 ||
      common.revision() < 0)
    return failure("InvalidHudViewSource", "hud_view");
  auto wallet = common.wallet(actor->wallet_id);
  if (!wallet || wallet->id != actor->wallet_id || wallet->balance < 0 ||
      wallet->owner.kind != Owner::Kind::actor || wallet->owner.id != player)
    return failure("InvalidHudViewSource", "hud_view");
  CommonPresentation presentation(common.state(), *world.catalog->common);
  auto name = presentation.display_name(player);
  if (!name.ok)
    return propagate(name);
  if (player.size() > 4096 || name.value.size() > 4096)
    return failure("JourneyViewTextLimit", "hud_view");
  Integer hp = actor->hp;
  if (auto id = args.find("personal_battle_id")) {
    auto selected = boundary::string(*id, "args/personal_battle_id");
    if (!selected.empty()) {
      auto *battle = world.personal.battles.find(selected);
      auto *unit = battle ? battle->units.find(player) : nullptr;
      if (!battle || battle->id != selected || battle->phase == "closed" ||
          !unit || unit->hp < 0 || unit->hp > 2147483647)
        return failure("InvalidHudViewSource", "hud_view");
      hp = unit->hp;
    }
  }
  return {true,
          object({{"ok", V{true}},
                  {"actor_id", V{player}},
                  {"revision", V{common.revision()}},
                  {"day_index", V{common.tick() / 2}},
                  {"half_day", V{common.tick() % 2}},
                  {"display_name", V{std::move(name.value)}},
                  {"balance", V{wallet->balance}},
                  {"hp", V{hp}}}),
          {}};
}
// 本人当前行动决定原学习/军训分支；两个领域各自投影真实课时、费用和公开练习范围。
Outcome<V> active_study(const WorldBundle &world) {
  auto *player =
      world.common.actor(world.common.state().metadata.player_actor_id);
  auto *action = player ? world.common.action(player->action_id) : nullptr;
  if (action && action->kind == "military_train")
    return {
        true, military_training_active_view(world.common, world.military), {}};
  auto access = world.services.growth_access();
  return query_growth_view(world.common, world.growth, *world.catalog->growth,
                           *world.catalog->common, world.social,
                           world.interaction, access,
                           world.common.state().metadata.player_actor_id,
                           "study_activity", object({}));
}
// 原围城选择与观测都由军事owner生成；本层只借同候选上下文，不复制空页或军团权威。
Outcome<V> current_siege(const WorldBundle &world) {
  auto interaction = world.services.interaction_access();
  auto access = world.services.military_access();
  access.interaction_access = &interaction;
  auto integration = siege_population_read_access(world);
  return military_siege_page(world.common, world.military,
                             *world.catalog->military, *world.catalog->common,
                             access, {}, &integration);
}
// 可选actor参数只能确认当前控制人，不能把有限用户页变成任意人物查询口。
Status current_actor(const WorldBundle &world, const V &args) {
  if (auto *value = args.find("actor_id"))
    if (boundary::string(*value, "args/actor_id") !=
        world.common.state().metadata.player_actor_id)
      return common_failure("NotPlayerActor");
  return common_success();
}
} // namespace
Outcome<BoundaryValue> query_live_player_view(const WorldBundle &world,
                                              const StableId &query,
                                              const BoundaryValue &args) {
  try {
    const auto &player = world.common.state().metadata.player_actor_id;
    CommonPresentation presentation(world.common.state(),
                                    *world.catalog->common);
    if (query == "political_field_appointments") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_political_field_appointment_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    // 新登记profile的有限观察不改变旧页面能力；游标绑定同一世界和观察者。
    if (query == "world_observation") {
      if (!world.catalog->director ||
          !world.catalog->director->dynamic_subjects)
        return failure("WorldObservationUnavailable", query,
                       ErrorKind::unsupported_capability);
      boundary::fields(args, {"place_id"}, {"cursor"}, "args");
      std::optional<WorldObservationCursor> cursor;
      if (const auto *value = args.find("cursor")) {
        auto decoded = decode_world_observation_cursor(*value);
        if (!decoded.ok)
          return propagate(decoded);
        cursor = std::move(decoded.value);
      }
      auto page = query_world_observation(
          world.common, world.social, world.tasks, world.interaction,
          world.observation_index, player,
          boundary::get_string(args, "place_id", "args"), cursor);
      if (!page.ok)
        return propagate(page);
      return {true, encode_world_observation(page.value), {}};
    }
    // 地图人物仅当前控制人精确同地，不接受远方place或任意actor查询参数。
    // 本人军团只走原active成员资格，空参数不开放远方或任意人物扫描。
    if (query == "map_company") {
      boundary::fields(args, {}, {}, "args");
      auto page =
          query_map_company(world.common, world.military, presentation, player);
      if (!page.ok)
        return propagate(page);
      return {true, encode_map_company(page.value), {}};
    }
    if (query == "map_presence") {
      boundary::fields(args, {}, {"cursor"}, "args");
      std::optional<MapPresenceCursor> cursor;
      if (const auto *value = args.find("cursor")) {
        auto decoded = decode_map_presence_cursor(*value);
        if (!decoded.ok)
          return propagate(decoded);
        cursor = std::move(decoded.value);
      }
      auto page = query_map_presence(world.common, *world.catalog->travel,
                                     world.services, presentation,
                                     world.map_presence_index, player, cursor);
      if (!page.ok)
        return propagate(page);
      return {true, encode_map_presence(page.value), {}};
    }
    if (query == "politics_entries" || query == "political_fiscal_entries") {
      boundary::fields(args, {"place_id"}, {}, "args");
      const auto site = boundary::get_string(args, "place_id", "args");
      if (query == "politics_entries") {
        if (!world.politics_enabled())
          return failure("PoliticalProfileRequired", query,
                         ErrorKind::unsupported_capability);
        PoliticalNavigationRead navigation{world.travel, *world.catalog->travel,
                                           world.services};
        auto page = query_political_business(world.politics_read(&navigation),
                                             player, site);
        if (!page.ok)
          return page;
        return merge_political_military_appointment_world_page(
            world, player, site, std::move(page.value));
      }
      if (!world.political_fiscal_enabled())
        return failure("PoliticalFiscalProfileRequired", query,
                       ErrorKind::unsupported_capability);
      return query_political_fiscal_entries(
          world.common, *world.catalog->common, world.politics,
          world.political_fiscal, *world.catalog->political_fiscal,
          world.organization, *world.catalog->organization, world.services,
          player, site, &world.politics_view_index);
    }
    if (query == "self_profile") {
      boundary::fields(args, {}, {}, "args");
      return self_profile_view(world, player);
    }
    if (query == "regional_life_topics") {
      if (!world.regional_life_enabled())
        return failure("RegionalLifeUnavailable", query,
                       ErrorKind::unsupported_capability);
      boundary::fields(args, {"place_id"}, {"cursor"}, "args");
      auto access = world.services.interaction_access();
      return query_regional_life_topics(
          world.common, world.interaction, *world.catalog->interaction, access,
          world.services, world.social, player,
          boundary::get_string(args, "place_id", "args"),
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "named_membership_entries") {
      if (!world.named_membership_enabled())
        return failure("NamedMembershipUnavailable", query,
                       ErrorKind::unsupported_capability);
      boundary::fields(args, {"place_id"}, {"provider_actor_id"}, "args");
      auto issued = PoliticalTakeoverExecutionWorldAccess::create(world);
      if (!issued.ok)
        return {false, {}, issued.error};
      return query_named_membership_entries(
          world.read(), *world.catalog, world.services, world.services, player,
          boundary::get_string(args, "place_id", "args"),
          args.find("provider_actor_id")
              ? boundary::get_string(args, "provider_actor_id", "args")
              : StableId{},
          [&world, &player](const OrganizationContractCommand &command) {
            return preview_organization_contract(world, player, command).ok;
          },
          issued.value->issued_supply());
    }
    if (query == "dynamic_place" || query == "dynamic_contract") {
      if (!world.dynamic_enabled())
        return failure("DynamicProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto authority = world.services.organization_access();
      if (query == "dynamic_place") {
        boundary::fields(args, {"place_id"}, {"cursor"}, "args");
        auto issued = PoliticalTakeoverExecutionWorldAccess::create(world);
        if (!issued.ok)
          return {false, {}, issued.error};
        return query_dynamic_place(
            world.read(), *world.catalog, authority, player,
            boundary::get_string(args, "place_id", "args"),
            args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0,
            [&world, &player](const OrganizationContractCommand &command) {
              return preview_organization_contract(world, player, command).ok;
            },
            issued.value->issued_supply());
      }
      boundary::fields(args, {"task_id"}, {}, "args");
      return query_dynamic_contract(
          world.read(), *world.catalog, authority, player,
          boundary::get_string(args, "task_id", "args"));
    }
    if (query == "journey_location") {
      boundary::fields(args, {}, {}, "args");
      auto result = query_journey_location(world.common, player);
      if (!result.ok)
        return propagate(result);
      return {true, location(result.value, true), {}};
    }
    if (query == "journey_map_position") {
      boundary::fields(args, {}, {}, "args");
      auto result = query_journey_map_position(
          world.common, *world.catalog->travel, world.services, player);
      if (!result.ok)
        return propagate(result);
      D fields{{"world_map_id", V{result.value.world_map_id}}};
      if (result.value.road) {
        const auto &r = *result.value.road;
        A points;
        for (const auto &p : r.points)
          points.push_back(point(p));
        fields.emplace_back("road_position",
                            object({{"edge_id", V{r.edge_id}},
                                    {"progress", V{r.progress}},
                                    {"length", V{r.length}},
                                    {"points", V{std::move(points)}},
                                    {"raw", V{r.raw}}}));
      } else
        fields.emplace_back("observer_position",
                            point(result.value.observer_position));
      return {true, V{std::move(fields)}, {}};
    }
    if (query == "journey_travel") {
      boundary::fields(args, {}, {}, "args");
      auto result = query_journey_travel(world.common, world.travel, player);
      if (!result.ok)
        return propagate(result);
      D fields{{"active", V{result.value.active}}};
      if (result.value.terrain) {
        auto &t = *result.value.terrain;
        fields.emplace_back("paused", V{t.paused});
        fields.emplace_back("remaining_ticks", V{t.remaining_ticks});
        fields.emplace_back("destination", destination(t.destination));
      } else if (result.value.road) {
        const auto &r = *result.value.road;
        fields.emplace_back("edge_id", V{r.edge_id});
        fields.emplace_back("progress", V{r.progress});
        fields.emplace_back("paused", V{r.paused});
      }
      return {true, V{std::move(fields)}, {}};
    }
    if (query == "journey_route") {
      boundary::fields(args, {}, {"cursor"}, "args");
      auto cursor =
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0;
      auto result =
          query_journey_route(world.common, world.travel, player, cursor);
      if (!result.ok)
        return propagate(result);
      D fields{{"active", V{result.value.active}}};
      page_fields(fields, result.value);
      if (result.value.road) {
        fields.emplace_back("kind", V{StableId{"road"}});
        fields.emplace_back("returning", V{result.value.returning});
        A path;
        for (const auto &id : result.value.path)
          path.push_back(V{id});
        fields.emplace_back("path", V{std::move(path)});
      } else {
        A points;
        for (auto &value : result.value.points)
          points.push_back(point(value));
        fields.emplace_back("points", V{std::move(points)});
      }
      return {true, V{std::move(fields)}, {}};
    }
    if (query == "journey_crossings") {
      boundary::fields(args, {}, {"cursor"}, "args");
      auto cursor =
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0;
      auto result = query_journey_crossings(
          *world.catalog->travel, world.services, cursor, world.common.tick());
      if (!result.ok)
        return propagate(result);
      D fields;
      page_fields(fields, result.value);
      A rows;
      if (result.value.road) {
        fields.emplace_back("kind", V{StableId{"road"}});
        for (auto &value : result.value.rows)
          rows.push_back(object(
              {{"route_id", V{value.flag_id}}, {"open", V{!value.closed}}}));
      } else
        for (auto &value : result.value.rows)
          rows.push_back(object(
              {{"flag_id", V{value.flag_id}}, {"closed", V{value.closed}}}));
      fields.emplace_back("rows", V{std::move(rows)});
      return {true, V{std::move(fields)}, {}};
    }
    if (query == "journey_companions") {
      boundary::fields(args, {}, {"cursor"}, "args");
      auto cursor =
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0;
      auto result = query_journey_companions(
          world.common.state(), world.services, presentation, player, cursor);
      if (!result.ok)
        return propagate(result);
      D fields{{"party_present", V{result.value.party_present}}};
      page_fields(fields, result.value);
      A rows;
      for (auto &value : result.value.rows)
        rows.push_back(object({{"actor_id", V{value.actor_id}},
                               {"display_name", V{value.display_name}},
                               {"cost_per_tick", V{value.cost_per_tick}}}));
      fields.emplace_back("rows", V{std::move(rows)});
      return {true, V{std::move(fields)}, {}};
    }
    if (query == "character_input") {
      boundary::fields(args, {"actor_id"}, {}, "args");
      const auto requested = boundary::get_string(args, "actor_id", "args");
      CommonCharacterPresentation source(world.common.state(),
                                         *world.catalog->common);
      auto result = query_selected_character(
          world.common.state(), world.interaction, *world.catalog->interaction,
          source, requested);
      if (result.ok)
        return encode_character_presentation(result.value);
      if (result.error.original.code != "InvalidPresentationActor")
        return propagate(result);
      return query_scene_character(world.common, world.interaction,
                                   *world.catalog->interaction,
                                   *world.catalog->common, requested);
    }
    if (query == "hud_view")
      return hud(world, args);
    if (query == "personal_battle_view") {
      boundary::fields(args, {}, {}, "args");
      return {true,
              personal_battle_view(world.common, world.personal,
                                   *world.catalog->personal,
                                   *world.catalog->common),
              {}};
    }
    if (query == "personal_battle_context") {
      boundary::fields(args, {"battle_id"}, {}, "args");
      return {true,
              personal_battle_context(
                  world.common, world.personal,
                  boundary::get_string(args, "battle_id", "args")),
              {}};
    }
    if (query == "personal_battle_geometry") {
      boundary::fields(args, {"battle_id", "draft"}, {}, "args");
      const auto id = boundary::get_string(args, "battle_id", "args");
      auto stamp = personal_battle_context(world.common, world.personal, id);
      if (auto *ok = stamp.find("ok");
          !ok || !ok->as<bool>() || !*ok->as<bool>())
        return {true, std::move(stamp), {}};
      auto draft = boundary::field(args, "draft", "args");
      if (!draft.as<D>())
        return failure("InvalidPlan", query);
      if (auto *key = draft.find("battle_id"))
        *key = V{id};
      else
        draft.as<D>()->emplace_back("battle_id", V{id});
      auto decoded = decode_personal_command("battle_plan", draft);
      if (!decoded.ok || !decoded.value.plan ||
          decoded.value.input_error.has_value())
        return failure("InvalidPlan", query);
      return {true,
              personal_battle_geometry(world.common, world.personal,
                                       *world.catalog->personal, id,
                                       *decoded.value.plan),
              {}};
    }
    if (query == "naval_battle_page" || query == "naval_battle_draft") {
      if (!world.naval_combat_enabled())
        return failure("NavalCombatProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      auto ma = world.services.military_access();
      ma.interaction_access = &ia;
      if (query == "naval_battle_page") {
        boundary::fields(args, {}, {"battle_id"}, "args");
        auto id = args.find("battle_id")
                      ? boundary::get_string(args, "battle_id", "args")
                      : StableId{};
        return id.empty()
                   ? naval_current_battle_page(
                         world.common, world.military, *world.catalog->military,
                         ma, world.naval, *world.catalog->naval, player,
                         naval_preview_access(world))
                   : naval_battle_page(world.common, world.military,
                                       *world.catalog->military, ma,
                                       world.naval, *world.catalog->naval,
                                       player, id, naval_preview_access(world));
      }
      boundary::fields(args, {"battle_id", "selected_id", "orders"}, {},
                       "args");
      auto orders = naval_battle_codec::decode_orders(
          boundary::field(args, "orders", "args"));
      if (!orders.ok)
        return propagate(orders);
      NavalPlanCommand plan{boundary::get_string(args, "battle_id", "args"),
                            "naval-draft:" +
                                std::to_string(world.common.revision()),
                            std::move(orders.value),
                            {}};
      return naval_battle_draft_page(
          world.common, world.military, *world.catalog->military, ma,
          world.naval, *world.catalog->naval, player, plan,
          boundary::get_string(args, "selected_id", "args"));
    }
    if (query == "military_battle_page") {
      boundary::fields(args, {}, {"battle_id"}, "args");
      return military_battle_page(
          world.common, world.military, *world.catalog->military,
          *world.catalog->common,
          args.find("battle_id")
              ? boundary::get_string(args, "battle_id", "args")
              : StableId{});
    }
    if (query == "military_battle_draft") {
      boundary::fields(args, {"battle_id", "selected_id", "orders"}, {},
                       "args");
      auto id = boundary::get_string(args, "battle_id", "args");
      auto decoded = decode_military_command(
          "military_plan",
          object({{"battle_id", V{id}},
                  {"orders", boundary::field(args, "orders", "args")}}));
      if (!decoded.ok)
        return propagate(decoded);
      auto *plan = std::get_if<MilitaryBattlePlan>(&decoded.value);
      if (!plan || !plan->orders_valid)
        return failure("InvalidMilitaryOrders", query);
      return military_battle_draft(
          world.common, world.military, *world.catalog->military, id,
          boundary::get_string(args, "selected_id", "args"), plan->orders);
    }
    if (query == "military_camp_provider_context") {
      boundary::fields(args, {}, {}, "args");
      return {true,
              military_camp_provider_context(world.common,
                                             *world.catalog->military,
                                             *world.catalog->common),
              {}};
    }
    if (query == "military_camp_page") {
      boundary::fields(args, {}, {"tab", "selection", "provider_context"},
                       "args");
      const auto tab = args.find("tab")
                           ? boundary::get_string(args, "tab", "args")
                           : StableId("orders");
      const auto selection =
          args.find("selection") ? *args.find("selection") : object({});
      const auto context = args.find("provider_context")
                               ? *args.find("provider_context")
                               : object({});
      boundary::object(selection, "args/selection");
      boundary::object(context, "args/provider_context");
      auto result = military_camp_page(
          world.common, world.military, *world.catalog->military,
          *world.catalog->common, world.estate, tab, selection, context);
      if (!result.ok)
        return result;
      if (world.naval_enabled()) {
        auto ia = world.services.interaction_access();
        auto ma = world.services.military_access();
        ma.interaction_access = &ia;
        auto naval = naval_camp_page(
            world.common, world.military, *world.catalog->military, ma,
            world.naval, *world.catalog->naval,
            world.common.state().metadata.player_actor_id,
            world.naval_combat_enabled());
        if (!naval.ok)
          return naval;
        auto *page = result.value.as<V::Dict>();
        if (!page)
          return {false, {}, common_failure("InvalidMilitaryCampView").error};
        page->emplace_back("naval", std::move(naval.value));
      }
      if (world.military_recovery_enabled()) {
        auto access = world.services.military_access();
        auto recovery = military_wounded_recovery_page(
            world.common, world.military, *world.catalog->military, access,
            world.common.state().metadata.player_actor_id);
        if (!recovery.ok)
          return recovery;
        auto *page = result.value.as<V::Dict>();
        if (!page)
          return {false, {}, common_failure("InvalidMilitaryCampView").error};
        page->emplace_back("wounded_recovery", std::move(recovery.value));
      }
      if (world.military_supply_attrition_enabled()) {
        auto access = world.services.military_access();
        auto supply = military_supply_attrition_page(
            world.common, world.military, *world.catalog->military, access,
            world.common.state().metadata.player_actor_id);
        if (!supply.ok)
          return supply;
        auto *page = result.value.as<V::Dict>();
        if (!page)
          return {false, {}, common_failure("InvalidMilitaryCampView").error};
        page->emplace_back("supply_attrition", std::move(supply.value));
      }
      if (world.military_tactic_learning_enabled()) {
        auto access = world.services.military_access();
        auto learned = military_tactic_learning_page(
            world.common, world.military, *world.catalog->military,
            world.military_tactic_learning, access,
            world.common.state().metadata.player_actor_id);
        if (!learned.ok)
          return learned;
        result.value.as<V::Dict>()->emplace_back("tactic_learning",
                                                 std::move(learned.value));
      }
      if (!world.population_military_enabled())
        return result;
      auto population = population_military_page(
          world.population_read(),
          world.common.state().metadata.player_actor_id);
      if (!population.ok)
        return population;
      auto *value = result.value.as<V::Dict>();
      if (!value)
        return {false, {}, common_failure("InvalidMilitaryCampView").error};
      value->emplace_back("population_military", std::move(population.value));
      return result;
    }
    if (query == "military_camp_offer") {
      boundary::fields(args, {"kind", "params"}, {}, "args");
      const auto kind = boundary::get_string(args, "kind", "args");
      const auto &params = boundary::field(args, "params", "args");
      boundary::object(params, "args/params");
      // 原营务报价和随后行军报价都必须复用中央只读守卫，不能只调用领域preview绕过占用/角色门。
      MilitaryCampPreview preview = [&](const StableId &command,
                                        const V &fields,
                                        const StableId &actor) -> Outcome<V> {
        Outcome<std::shared_ptr<const PreviewResult>> quote;
        auto guard = command_common_guard(world, actor, command);
        if (!guard.ok)
          quote = {false, {}, guard.error};
        else {
          auto decoded = decode_resident_command(command, fields);
          quote = decoded.ok ? prepare_command(world, actor, decoded.value)
                             : Outcome<std::shared_ptr<const PreviewResult>>{
                                   false, {}, decoded.error};
        }
        return encode_resident_preview(quote, world.common.revision());
      };
      return military_camp_offer(world.common, world.military,
                                 *world.catalog->military,
                                 *world.catalog->common, *world.catalog->travel,
                                 kind, params, preview);
    }
    if (query == "siege_page") {
      boundary::fields(args, {}, {}, "args");
      return current_siege(world);
    }
    if (query == "siege_draft") {
      boundary::fields(args, {"identity", "requests"}, {"first_ok"}, "args");
      const auto &identity = boundary::field(args, "identity", "args");
      const auto &requests = boundary::array(
          boundary::field(args, "requests", "args"), "args/requests");
      auto interaction = world.services.interaction_access();
      auto access = world.services.military_access();
      access.interaction_access = &interaction;
      return military_siege_draft(
          world.common, world.military, *world.catalog->military, access,
          identity, requests,
          args.find("first_ok") ? boundary::get_bool(args, "first_ok", "args")
                                : false);
    }
    // 当面委托只允许当前本人和当前会话；领域叶核本人线索与原命令报价。
    if (query == "political_occupation_fiscal") {
      boundary::fields(args, {"place_id"}, {}, "args");
      return query_occupation_fiscal_world(
          world, world.common.state().metadata.player_actor_id,
          boundary::get_string(args, "place_id", "args"));
    }
    if (query == "political_takeover") {
      boundary::fields(args, {"place_id"}, {}, "args");
      if (!world.political_takeover_enabled())
        return failure("PoliticalTakeoverProfileRequired", query,
                       ErrorKind::unsupported_capability);
      return query_political_takeover_world(
          world, *world.catalog->political_takeover, world.political_takeover,
          world.common.state().metadata.player_actor_id,
          boundary::get_string(args, "place_id", "args"));
    }
    if (query == "diplomacy") {
      boundary::fields(args, {"place_id"}, {}, "args");
      if (!world.diplomacy_enabled())
        return failure("DiplomacyProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      DiplomacyNavigationRead navigation{world.travel, *world.catalog->travel,
                                         world.services};
      auto page = query_diplomacy_business(
          world.diplomacy_read(ia, &navigation),
          world.common.state().metadata.player_actor_id,
          boundary::get_string(args, "place_id", "args"));
      if (!page.ok)
        return page;
      return merge_treaty_passage_world_page(
          world, world.common.state().metadata.player_actor_id,
          std::move(page.value));
    }
    if (query == "politics_governance") {
      boundary::fields(args, {"place_id"}, {}, "args");
      if (!world.governance_tasks_enabled())
        return failure("GovernanceNotEnabled", query,
                       ErrorKind::unsupported_capability);
      return query_governance_business(
          world.governance_read(),
          world.common.state().metadata.player_actor_id,
          boundary::get_string(args, "place_id", "args"));
    }
    if (query == "economic_supply_policies") {
      auto query = decode_local_economy_query(args, player);
      if (!query.ok)
        return {false, {}, query.error};
      return query_economic_supply_world(world, query.value);
    }
    if (query == "local_economy")
      return query_economic_world(world, world.economic_index, args);
    if (query == "household_provision_management")
      return query_household_provision_world(
          world, world.household_provision_index, args);
    if (query == "local_production_history")
      return query_economic_production_history(world, world.economic_index,
                                               args);
    if (query == "occupation_fiscal_settlement")
      return query_occupation_fiscal_settlement_world(world, args);
    if (query == "fiscal_taxpayer_continuity")
      return query_fiscal_taxpayer_continuity_world(world, args);
    if (query == "fiscal_settlement")
      return query_economic_fiscal_world(world, world.fiscal_index, args);
    // 携运页只读本人真实托管货物和同行者，续页绑定完整当前身份。
    // 仅本人既有受托条款与目的城；未知额外参数拒绝，游标不跨世界/本人/目的地复用。
    if (query == "task_route_obligations") {
      boundary::fields(args, {"destination_settlement_id"}, {"cursor"}, "args");
      std::optional<TaskRouteObligationsCursor> cursor;
      if (const auto *value = args.find("cursor")) {
        auto decoded = decode_task_route_obligations_cursor(*value);
        if (!decoded.ok)
          return propagate(decoded);
        cursor = std::move(decoded.value);
      }
      Outcome<TaskRouteObligationsPage> page;
      if (world.envoy_tasks_enabled()) {
        DiplomaticMissionAccess missions(
            world.common, world.organization, *world.catalog->organization,
            *world.catalog->diplomacy, world.diplomacy);
        auto envoy_access = world.services.interaction_access();
        DiplomaticPresentRead report_context{world.common,
                                             world.organization,
                                             *world.catalog->organization,
                                             *world.catalog->diplomacy,
                                             world.diplomacy,
                                             world.interaction,
                                             *world.catalog->interaction,
                                             envoy_access};
        std::optional<DiplomaticReportAccess> report_source;
        if (world.envoy_tasks.version == 2) {
          if (!world.diplomacy.v2)
            return {false, {}, common_failure("MissingDiplomacyV2State").error};
          report_source.emplace(report_context, *world.diplomacy.v2);
        }
        page = query_task_route_obligations_with_envoy(
            world.common, world.tasks, *world.catalog->tasks,
            *world.catalog->common, world.observation_index, player,
            boundary::get_string(args, "destination_settlement_id", "args"),
            cursor,
            world.governance_tasks_enabled() ? &world.governance_tasks
                                             : nullptr,
            world.envoy_tasks, missions, missions,
            report_source ? &*report_source : nullptr);
      } else
        page = query_task_route_obligations(
            world.common, world.tasks, *world.catalog->tasks,
            *world.catalog->common, world.observation_index, player,
            boundary::get_string(args, "destination_settlement_id", "args"),
            cursor,
            world.governance_tasks_enabled() ? &world.governance_tasks
                                             : nullptr);
      if (!page.ok)
        return propagate(page);
      return {true, encode_task_route_obligations(page.value), {}};
    }
    if (query == "task_travel_manifest") {
      boundary::fields(args, {}, {"cursor"}, "args");
      std::optional<TaskTravelManifestCursor> cursor;
      if (auto value = args.find("cursor")) {
        auto decoded = decode_task_travel_manifest_cursor(*value);
        if (!decoded.ok)
          return propagate(decoded);
        cursor = std::move(decoded.value);
      }
      auto page = query_task_travel_manifest(
          world.common, world.tasks, *world.catalog->tasks,
          world.observation_index, presentation, player, cursor);
      if (!page.ok)
        return propagate(page);
      return {true, encode_task_travel_manifest(page.value), {}};
    }
    if (query == "visit_messages") {
      boundary::fields(args, {}, {"cursor", "offer_cursor"}, "args");
      return query_visit_message_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0,
          args.find("offer_cursor")
              ? boundary::get_int(args, "offer_cursor", "args")
              : 0);
    }
    if (query == "task_extensions") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_task_extension_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_message_returns") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_task_message_returns_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_personal_messages") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_task_personal_messages_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_personal_reports") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_task_personal_reports_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_personal_acceptances") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_task_personal_acceptances_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_personal_cancellations") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_task_personal_cancellations_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "known_contact_locations") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_known_contact_location_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "personal_appointments") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_personal_appointment_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "contact_inquiries") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_contact_inquiry_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "visit_message_replies") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_visit_reply_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "visit_appointments") {
      boundary::fields(args, {}, {"cursor", "offer_cursor"}, "args");
      return query_visit_appointment_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0,
          args.find("offer_cursor")
              ? boundary::get_int(args, "offer_cursor", "args")
              : 0);
    }
    if (query == "born_personal_dialogues") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_born_personal_dialogue_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "known_personal_dialogues") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_known_personal_dialogue_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "party_recruitment") {
      boundary::fields(args, {}, {}, "args");
      auto access = world.services.interaction_access();
      return query_party_recruitment_consent(
          world.common, world.interaction, *world.catalog->interaction,
          world.catalog->interaction->party_recruitment_consents.get(), access);
    }
    if (query == "party_participation") {
      boundary::fields(args, {}, {}, "args");
      if (!world.party_invitations_enabled())
        return failure("PartyInvitationsUnavailable", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      auto view = query_party_participation(
          world.common, world.interaction, *world.catalog->interaction,
          *world.catalog->interaction->party_invitations, ia,
          world.common.state().metadata.player_actor_id);
      if (!view.ok)
        return {false, {}, view.error};
      return {true, encode_party_participation(view.value), {}};
    }
    if (query == "task_static_assets") {
      boundary::fields(args, {}, {"cursor", "receipt_id", "lot_cursor"},
                       "args");
      if (!world.catalog->task_static_assets_enabled)
        return failure("TaskStaticAssetProfileRequired", query,
                       ErrorKind::unsupported_capability);
      if (!world.tasks.static_assets)
        return failure("MissingTaskStaticAssetDomain", query);
      return query_task_static_assets(
          world.common, world.tasks, *world.catalog->tasks,
          *world.tasks.static_assets,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0,
          args.find("receipt_id")
              ? boundary::get_string(args, "receipt_id", "args")
              : StableId{},
          args.find("lot_cursor")
              ? boundary::get_int(args, "lot_cursor", "args")
              : 0);
    }
    if (query == "task_message_handoffs") {
      boundary::fields(args, {}, {"cursor"}, "args");
      if (!world.catalog->task_message_handoffs_enabled)
        return failure("TaskMessageHandoffProfileRequired", query,
                       ErrorKind::unsupported_capability);
      if (!world.tasks.message_handoffs)
        return failure("MissingTaskMessageHandoffDomain", query);
      return query_task_message_handoffs(
          world.common, world.tasks, *world.catalog->tasks,
          *world.catalog->common, *world.tasks.message_handoffs,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_death_notices") {
      boundary::fields(args, {}, {"cursor"}, "args");
      if (!world.catalog->task_message_notices)
        return failure("TaskMessageNoticeProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto notice = make_task_message_notice_services(world);
      if (!notice.ok)
        return {false, {}, notice.error};
      if (!notice.value)
        return failure("MissingTaskMessageNoticeDomain", query);
      return query_delivered_task_death_notices(
          world.common, world.tasks, *world.catalog->tasks,
          *world.catalog->common, *world.catalog->task_message_notices,
          *world.tasks.message_notices, *world.tasks.message_handoffs,
          notice.value->validate,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_public_account_execution") {
      boundary::fields(args, {}, {"cursor"}, "args");
      if (!world.political_takeover_enabled())
        return failure("TaskTakeoverExecutionProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto takeover = PoliticalTakeoverExecutionWorldAccess::create(world);
      if (!takeover.ok)
        return {false, {}, takeover.error};
      return query_task_public_account_execution(
          world.common, world.tasks, *world.catalog->tasks,
          *takeover.value->responsibilities(),
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "task_personal_advances") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_task_advances_world(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "runtime_interaction_tasks") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_runtime_interaction_tasks(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "interaction_tasks") {
      boundary::fields(args, {}, {"cursor"}, "args");
      return query_interaction_tasks(
          world,
          args.find("cursor") ? boundary::get_int(args, "cursor", "args") : 0);
    }
    if (query == "journal_text") {
      boundary::fields(args, {}, {}, "args");
      if (world.envoy_tasks_enabled()) {
        DiplomaticMissionAccess missions(
            world.common, world.organization, *world.catalog->organization,
            *world.catalog->diplomacy, world.diplomacy);
        auto envoy_access = world.services.interaction_access();
        DiplomaticPresentRead report_context{world.common,
                                             world.organization,
                                             *world.catalog->organization,
                                             *world.catalog->diplomacy,
                                             world.diplomacy,
                                             world.interaction,
                                             *world.catalog->interaction,
                                             envoy_access};
        std::optional<DiplomaticReportAccess> report_source;
        if (world.envoy_tasks.version == 2) {
          if (!world.diplomacy.v2)
            return {false, {}, common_failure("MissingDiplomacyV2State").error};
          report_source.emplace(report_context, *world.diplomacy.v2);
        }
        return query_task_journal_with_responsibilities(
            world.common, world.tasks, *world.catalog->tasks,
            *world.catalog->common, world.interaction, world.social,
            world.growth, world.governance_tasks,
            world.governance_tasks_enabled(), world.envoy_tasks, true, missions,
            missions, report_source ? &*report_source : nullptr);
      }
      if (world.governance_tasks_enabled())
        return query_task_journal_with_governance(
            world.common, world.tasks, *world.catalog->tasks,
            *world.catalog->common, world.interaction, world.social,
            world.growth, world.governance_tasks, true);
      return query_task_journal(world.common, world.tasks,
                                *world.catalog->tasks, *world.catalog->common,
                                world.interaction, world.social, world.growth);
    }
    if (query == "private_shop_liquidations") {
      auto q = decode_private_shop_liquidation_query(args);
      if (!q.ok)
        return {false, {}, q.error};
      return query_private_shop_liquidation_world(world, q.value);
    }
    if (query == "private_shop_daily_rent") {
      auto parsed = decode_private_shop_daily_rent_query(args);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      return query_private_shop_daily_rent_world(world, parsed.value);
    }
    if (query == "private_shop_business") {
      auto parsed = decode_private_shop_business_query(args);
      if (!parsed.ok)
        return {false, {}, parsed.error};
      return query_private_shop_world(world, parsed.value);
    }
    if (query == "political_shop_registry_successions") {
      auto unique = validate_economic_payload_fields(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_shop_registry_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_governance_continuations") {
      auto unique = validate_economic_payload_fields(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_governance_continuation_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "governance_invitation_replies") {
      auto unique = validate_economic_payload_fields(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_governance_invitation_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_head_handovers") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_head_handover_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_successor_rounds") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_successor_round_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_term_handovers") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_term_handover_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_office_receptions") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_office_reception_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_office_resignations") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_office_resignation_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_subordinate_appointments") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_subordinate_appointment_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_governance_agencies") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_governance_agency_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_successions") {
      auto guard = validate_economic_payload_fields(args);
      if (!guard.ok)
        return {false, {}, guard.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_succession_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "political_shop_licensing") {
      auto unique = validate_economic_payload_fields(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"place_id", "cursor"}, {}, "args");
      return query_political_shop_licensing_world(
          world, boundary::get_string(args, "place_id", "args"),
          boundary::get_int(args, "cursor", "args"));
    }
    if (query == "task_shop_orders") {
      auto unique = validate_economic_payload_fields(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"cursor", "order_id"}, {}, "args");
      return query_task_shop_orders_world(
          world, boundary::get_int(args, "cursor", "args"),
          boundary::get_string(args, "order_id", "args"));
    }
    if (query == "task_employment") {
      auto unique = validate_economic_payload_fields(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"cursor", "policy_cursor"}, {}, "args");
      return query_estate_employment_world(
          world, boundary::get_int(args, "cursor", "args"),
          boundary::get_int(args, "policy_cursor", "args"));
    }
    if (query == "growth_tactic_lesson_view") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"page"}, {}, "args");
      return query_growth_tactic_lesson_world(
          world, boundary::get_int(args, "page", "args"));
    }
    if (query == "personal_recovery") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"page"}, {}, "args");
      return query_growth_recovery_world(
          world, boundary::integer(boundary::field(args, "page", "args"),
                                   "args.page"));
    }
    if (query == "family_care_relief_page") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"page"}, {}, "args");
      if (!world.family_care_relief_enabled())
        return failure("ChildCareReliefProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      return query_child_care_relief_page(
          world.common, *world.catalog->common, world.interaction,
          *world.catalog->interaction, ia, *world.catalog->family_care_relief,
          world.family_birth, world.family_life, world.child_care_relief,
          world.family_care_relief_access(),
          boundary::get_int(args, "page", "args"));
    }
    if (query == "family_birth_page") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"page"}, {}, "args");
      if (!world.family_birth_enabled())
        return failure("BirthProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      return query_family_birth_page(
          world.common, *world.catalog->common, world.interaction,
          *world.catalog->interaction, ia, *world.catalog->family,
          *world.catalog->family_birth, world.family_birth,
          world.family_birth_access(), boundary::get_int(args, "page", "args"));
    }
    if (query == "family_apprenticeship") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"page"}, {}, "args");
      if (!world.family_apprenticeship_enabled())
        return failure("ApprenticeshipProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      return query_apprenticeship_page(
          world.common, *world.catalog->common, world.interaction,
          *world.catalog->interaction, ia,
          *world.catalog->family_apprenticeship, world.family_apprenticeship,
          world.family_apprenticeship_access(),
          boundary::get_int(args, "page", "args"));
    }
    if (query == "family_apprenticeship_courses") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"bond_id", "page"}, {}, "args");
      if (!world.family_apprenticeship_enabled())
        return failure("ApprenticeshipProfileRequired", query,
                       ErrorKind::unsupported_capability);
      return query_apprenticeship_courses_page(
          world.common, world.growth, world.family_apprenticeship,
          boundary::get_string(args, "bond_id", "args"),
          boundary::get_int(args, "page", "args"));
    }
    if (query == "family_marriage_page") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"page"}, {}, "args");
      if (!world.family_marriage_enabled())
        return failure("MarriageProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      return query_family_marriage_page(
          world.common, *world.catalog->common, world.family, world.family_life,
          world.interaction, *world.catalog->interaction, ia,
          *world.catalog->family_marriage, world.family_marriage,
          world.family_marriage_access(),
          boundary::get_int(args, "page", "args"));
    }
    if (query == "family_inheritance_page") {
      auto unique = validate_succession_json_unique(args);
      if (!unique.ok)
        return {false, {}, unique.error};
      boundary::fields(args, {"page", "lot_page", "notice_binding_fact_id"}, {},
                       "args");
      if (!world.family_inheritance_enabled())
        return failure("FamilyInheritanceProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      return query_family_inheritance_page(
          world.common, *world.catalog->common, world.family_bequest,
          world.family_life, world.interaction, *world.catalog->interaction, ia,
          *world.catalog->family_inheritance, world.family_inheritance,
          world.family_inheritance_access(),
          boundary::get_int(args, "page", "args"),
          boundary::get_int(args, "lot_page", "args"),
          boundary::get_string(args, "notice_binding_fact_id", "args"));
    }
    if (query == "family_bequest_page") {
      boundary::fields(args, {"page"}, {}, "args");
      if (!world.family_bequest_enabled())
        return failure("FamilyBequestProfileRequired", query,
                       ErrorKind::unsupported_capability);
      return query_family_bequest_page(
          world.common, world.family, world.family_life,
          world.family_death_read(), *world.catalog->family_bequest,
          world.family_bequest, world.family_bequest_player_read(),
          *world.catalog->common, boundary::get_int(args, "page", "args"));
    }
    if (query == "family_voluntary_paths") {
      boundary::fields(args, {"page"}, {}, "args");
      return query_family_voluntary_paths_world(
          world, boundary::get_int(args, "page", "args"));
    }
    if (query == "family_collateral_page") {
      boundary::fields(args, {"page"}, {}, "args");
      return query_family_collateral_world(
          world, boundary::get_int(args, "page", "args"));
    }
    if (query == "family_biological_page") {
      boundary::fields(args, {"page"}, {}, "args");
      return query_family_biological_world(
          world, boundary::get_int(args, "page", "args"));
    }
    if (query == "family_legacy_page") {
      boundary::fields(args, {"page"}, {}, "args");
      if (!world.family_legacy_enabled())
        return failure("LegacyControlConsentProfileRequired", query,
                       ErrorKind::unsupported_capability);
      auto ia = world.services.interaction_access();
      return query_family_legacy_world(world,
                                       boundary::get_int(args, "page", "args"));
    }
    if (query == "family_death_page") {
      boundary::fields(args, {}, {}, "args");
      if (!world.family_life_enabled())
        return failure("FamilyLifeCapabilityRequired", query,
                       ErrorKind::unsupported_capability);
      return query_family_death_page(world.common, world.family_life,
                                     world.family_death_read());
    }
    if (query == "voluntary_succession_page") {
      boundary::fields(args, {}, {"page"}, "args");
      auto interaction = world.services.interaction_access();
      auto family_sources = NativeFamilyInitialSourcesLease::create(world);
      if (!family_sources.ok)
        return {false, {}, std::move(family_sources.error)};
      auto access = world.services.family_access(
          interaction,
          family_sources.value ? &family_sources.value->origins() : nullptr,
          family_sources.value ? &family_sources.value->initial_age()
                               : nullptr);
      return query_voluntary_succession_page(
          world.common, world.family, *world.catalog->family,
          *world.catalog->common, world.estate, *world.catalog->estate, access,
          args.find("page") ? boundary::get_int(args, "page", "args") : 0);
    }
    if (query == "adult_adoption_requests") {
      boundary::fields(args, {}, {"page"}, "args");
      if (!world.catalog->family->adult_adoption)
        return failure("AdultAdoptionUnavailable", query,
                       ErrorKind::unsupported_capability);
      auto access = world.services.interaction_access();
      return query_adult_adoption_page(
          world.common, world.family, world.interaction,
          *world.catalog->interaction, access,
          *world.catalog->family->adult_adoption,
          args.find("page") ? boundary::get_int(args, "page", "args") : 0);
    }
    if (query == "family_page" || query == "family_provider_context") {
      auto growth_access = world.services.growth_access();
      FamilyRestRows rests = [&](const StableId &actor) {
        return growth_rest_rows(world.common, world.growth,
                                *world.catalog->growth, growth_access, actor);
      };
      return query_family_view(world.common, world.family,
                               *world.catalog->common, query, args, rests);
    }
    if (query == "estate_available") {
      boundary::fields(args, {"estate_id"}, {}, "args");
      auto id = boundary::get_string(args, "estate_id", "args");
      return {true,
              object({{"ok", V{true}},
                      {"available",
                       V{estate_available(world.common, world.estate,
                                          *world.catalog->estate, id)}}}),
              {}};
    }
    if (query == "estate_page") {
      boundary::fields(args, {"estate_id"}, {"tab"}, "args");
      return query_estate_page(
          world.common, world.estate, *world.catalog->estate,
          *world.catalog->common, world.services,
          boundary::get_string(args, "estate_id", "args"),
          args.find("tab") ? boundary::get_string(args, "tab", "args")
                           : StableId("people"));
    }
    if (query == "estate_inputs") {
      boundary::fields(args, {"estate_id"}, {}, "args");
      return query_estate_inputs(
          world.common, world.estate, *world.catalog->estate,
          boundary::get_string(args, "estate_id", "args"));
    }
    if (query == "scene_view") {
      boundary::fields(args, {"place_id", "view_kind"}, {}, "args");
      return query_scene_view(world.common, world.interaction,
                              *world.catalog->interaction, world.social,
                              world.estate, *world.catalog->estate,
                              *world.catalog->common,
                              boundary::get_string(args, "place_id", "args"),
                              boundary::get_string(args, "view_kind", "args"));
    }
    if (query == "place_sidebar") {
      boundary::fields(args, {}, {}, "args");
      auto access = world.services.interaction_access();
      return query_place_sidebar(
          world.common, world.interaction, *world.catalog->interaction, access,
          world.estate, *world.catalog->estate, world.military,
          *world.catalog->military, *world.catalog->common);
    }
    if (query == "study_activity") {
      boundary::fields(args, {}, {}, "args");
      return active_study(world);
    }
    if (query == "action_manual_window") {
      boundary::fields(args, {}, {}, "args");
      return {true,
              object({{"ok", V{true}},
                      {"manual", V{(world.naval_combat_enabled() &&
                                    world.services.in_open_battle(player)) ||
                                   military_manual_window(
                                       world.common, world.military, player) ||
                                   growth_manual_window(
                                       world.common, world.growth, player)}}}),
              {}};
    }
    if (query == "military_drill") {
      boundary::fields(args, {"action_id"}, {"actor_id"}, "args");
      auto checked = current_actor(world, args);
      if (!checked.ok)
        return propagate(checked);
      return military_drill_view(
          world.common, world.military, *world.catalog->military, player,
          boundary::get_string(args, "action_id", "args"));
    }
    if (query == "insight_cards") {
      boundary::fields(args, {}, {"actor_id", "discovered", "offset", "limit"},
                       "args");
      auto checked = current_actor(world, args);
      if (!checked.ok)
        return propagate(checked);
      std::vector<StableId> discovered;
      if (const auto *values = args.find("discovered")) {
        const auto &rows = boundary::array(*values, "args/discovered");
        if (rows.size() > 4096)
          return failure("InvalidInsightDiscovery");
        for (const auto &row : rows)
          discovered.push_back(boundary::string(row, "args/discovered"));
      }
      return growth_insight_cards_view(
          world.common.state(), *world.catalog->common, world.military,
          world.catalog->growth->insights, player, discovered,
          args.find("offset") ? boundary::get_int(args, "offset", "args") : 0,
          args.find("limit") ? boundary::get_int(args, "limit", "args") : 32);
    }
    if (query == "actor_cards") {
      boundary::fields(args, {}, {"actor_id", "offset", "limit"}, "args");
      auto checked = current_actor(world, args);
      if (!checked.ok)
        return propagate(checked);
      const auto &cards = world.catalog->growth->actor_start.cards;
      auto page = actor_card_page(
          world, player,
          args.find("offset") ? boundary::get_int(args, "offset", "args") : 0,
          args.find("limit") ? boundary::get_int(args, "limit", "args") : 32);
      if (!page.ok)
        return {false, {}, page.error};
      return encode_growth_actor_cards_page(page.value, cards,
                                            *world.catalog->common);
    }
    if (query == "study_teaching") {
      boundary::fields(args, {}, {"actor_id", "context", "page"}, "args");
      auto checked = current_actor(world, args);
      if (!checked.ok)
        return propagate(checked);
      std::optional<InteractionContext> context;
      if (const auto *value = args.find("context"))
        context = interaction_codec::decode_context(*value, "args/context");
      auto access = world.services.growth_access();
      return growth_study_teaching_view(
          world.common, world.growth, *world.catalog->growth,
          *world.catalog->common, world.social, access, player, context,
          args.find("page") ? boundary::get_int(args, "page", "args") : 0);
    }
    const bool growth_query =
        query == "growth_cards" || query == "ordinary_skills" ||
        query == "character_summary" || query == "study_courses" ||
        query == "study_challenge" || query == "study_etiquette" ||
        query == "study_course_contexts" || query == "trade_view" ||
        query == "trade_cache_context" || query == "trade_action_receipt" ||
        query == "trade_bargain" || query == "trade_bargain_view" ||
        query == "trade_bargain_source";
    if (growth_query) {
      if (query == "growth_cards")
        boundary::fields(args, {},
                         {"actor_id", "discovered", "offset", "limit"}, "args");
      else if (query == "ordinary_skills")
        boundary::fields(args, {}, {"skill_id"}, "args");
      else if (query == "character_summary")
        boundary::fields(args, {}, {"page"}, "args");
      else if (query == "study_courses")
        boundary::fields(args, {"context"}, {"actor_id", "ticks"}, "args");
      else if (query == "study_challenge" || query == "study_etiquette")
        boundary::fields(args, {"action_id"}, {"actor_id"}, "args");
      else if (query == "study_course_contexts" ||
               query == "trade_cache_context")
        boundary::fields(args, {}, {}, "args");
      else if (query == "trade_view")
        boundary::fields(args, {"shop_id", "provider_actor_id"},
                         {"receipt_action_id"}, "args");
      else if (query == "trade_action_receipt")
        boundary::fields(args, {"action_id"}, {}, "args");
      else if (query == "trade_bargain_source")
        boundary::fields(args, {"basis_sha256"}, {"actor_id"}, "args");
      else
        boundary::fields(args, {"quote_id"}, {"actor_id"}, "args");
      auto checked = current_actor(world, args);
      if (!checked.ok)
        return propagate(checked);
      auto access = world.services.growth_access();
      return query_growth_view(world.common, world.growth,
                               *world.catalog->growth, *world.catalog->common,
                               world.social, world.interaction, access, player,
                               query, args);
    }
    if (query == "screen_context") {
      boundary::fields(args, {}, {}, "args");
      auto raw = query_journey_location(world.common, player);
      if (!raw.ok)
        return propagate(raw);
      auto legacy = world_scene_legacy(world, player);
      if (!legacy.ok)
        return propagate(legacy);
      auto personal = personal_battle_view(world.common, world.personal,
                                           *world.catalog->personal,
                                           *world.catalog->common);
      auto military = military_battle_page(world.common, world.military,
                                           *world.catalog->military,
                                           *world.catalog->common);
      if (!military.ok)
        return military;
      auto siege = current_siege(world);
      if (!siege.ok)
        return siege;
      auto study = active_study(world);
      if (!study.ok)
        return study;
      auto output =
          object({{"ok", V{true}},
                  {"personal_battle_view", std::move(personal)},
                  {"military_battle_page", std::move(military.value)},
                  {"siege_page", std::move(siege.value)},
                  {"study_activity", std::move(study.value)},
                  {"presentation_location", location(raw.value, false)},
                  {"world_scene_legacy", V{legacy.value}}});
      if (world.naval_combat_enabled()) {
        auto ia = world.services.interaction_access();
        auto ma = world.services.military_access();
        ma.interaction_access = &ia;
        auto naval = naval_current_battle_page(
            world.common, world.military, *world.catalog->military, ma,
            world.naval, *world.catalog->naval, player,
            naval_preview_access(world));
        if (!naval.ok)
          return naval;
        output.as<V::Dict>()->emplace_back("naval_battle_page",
                                           std::move(naval.value));
      }
      if (world.catalog->family->character_creation) {
        auto mode = family_character_creation_mode(world.common, world.family,
                                                   *world.catalog->family,
                                                   *world.catalog->common);
        if (!mode.ok)
          return {false, {}, mode.error};
        output.as<V::Dict>()->emplace_back(
            "character_creation",
            object({{"mode", V{character_creation_mode_name(mode.value)}}}));
      }
      if (world.catalog->growth->cards.enabled) {
        if (!world.growth.cards)
          return failure("MissingGrowthCards");
        const auto *owned = world.growth.cards->owned.find(player);
        output.as<V::Dict>()->emplace_back(
            "growth_cards_summary",
            object(
                {{"enabled", V{true}},
                 {"actor_id", V{player}},
                 {"acquired_count", V{Integer(owned ? owned->size() : 0)}}}));
      }
      if (world.catalog->growth->insights.enabled) {
        auto lived = first_growth_camp_training_experience(
            world.common.state(), world.military, player);
        if (!lived.ok)
          return {false, {}, lived.error};
        output.as<V::Dict>()->emplace_back(
            "insight_cards_summary",
            object({{"enabled", V{true}},
                    {"actor_id", V{player}},
                    {"acquired_count",
                     V{Integer(lived.value ? world.catalog->growth->insights
                                                 .definitions.size()
                                           : 0)}}}));
      }
      if (world.catalog->growth->actor_start.cards.enabled) {
        const auto &cards = world.catalog->growth->actor_start.cards;
        auto page = actor_card_page(world, player, 0, 1);
        if (!page.ok)
          return {false, {}, page.error};
        const auto completed = page.value.cards.empty()
                                   ? Integer{0}
                                   : page.value.cards.front().completed_count;
        if (cards.version == 1)
          output.as<V::Dict>()->emplace_back(
              "actor_cards_summary",
              object({{"enabled", V{true}},
                      {"actor_id", V{player}},
                      {"completed_count", V{completed}},
                      {"acquired_count",
                       V{completed == 2 ? page.value.total : Integer{0}}}}));
        else
          output.as<V::Dict>()->emplace_back(
              "actor_cards_summary",
              object({{"enabled", V{true}},
                      {"actor_id", V{player}},
                      {"source_version", V{Integer{2}}},
                      {"total", V{page.value.total}},
                      {"acquired_count", V{page.value.acquired_total}}}));
      }
      return {true, std::move(output), {}};
    }
    return failure("UnsupportedLiveQuery", query,
                   ErrorKind::unsupported_capability);
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace hanmo::resident
