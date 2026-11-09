#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "field_event_mission_application.hpp"
#include "field_army_exit_selection.hpp"
#include "field_army_exit_route.hpp"
#include "field_author_data.hpp"
#include "field_binding_source.hpp"
#include "field_boundary_source.hpp"
#include "field_command_source.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_initial_placement_source.hpp"
#include "field_result_codec.hpp"
#include "field_roster_origin.hpp"
#include "resident/domains/family/family_life_history4.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_field_command_source.hpp"
#include "resident/domains/military/military_lifecycle.hpp"
#include "resident/domains/military/military_return_destination_policy.hpp"
#include "resident/domains/military/military_march_source.hpp"
#include "resident/domains/military/occupation_time.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 原time字段仅局部比较，不提升成世界M。
bool same_time(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
// 精确读同军唯一原开程；边界援军也沿自己的原来源，绝不借初始另一端。
SourceRef original_opening(const FieldExactContactReadContext &x,
                          const FieldBattleCatalog &catalog,
                          const FieldBattleRoadBinding &binding) {
  if (const auto *initial = std::get_if<FieldInitialContactBinding>(&binding.source)) {
    const auto contact = x.field.contacts.find(initial->contact_id);
    const auto *exact = contact == x.field.contacts.end() ? nullptr :
        std::get_if<FieldExactGroupContactOrigin>(&contact->second.source);
    if (!exact) fail("FieldArmyExitExactContactRequired", binding.army_id);
    const auto source = take(read_field_exact_road_contact_source(x, exact->actual_contact,
                                                                 catalog.parameters));
    for (const auto &endpoint : source.endpoints)
      if (endpoint.army->id == binding.army_id)
        return {"travel", "group_movement_opening", endpoint.opening->id};
  } else {
    const auto &id = std::get<FieldBoundaryContactBinding>(binding.source).contact_id;
    const auto contact = x.field.boundary_contacts.find(id);
    const auto *exact = contact == x.field.boundary_contacts.end() ? nullptr :
        std::get_if<FieldExactBoundaryOrigin>(&contact->second.motion);
    if (!exact) fail("FieldArmyExitExactBoundaryRequired", binding.army_id);
    const auto source = take(read_field_boundary_original_source(x, catalog, exact->actual_contact));
    if (source.army && source.army->id == binding.army_id && source.opening)
      return {"travel", "group_movement_opening", source.opening->id};
  }
  fail("FieldArmyExitOriginalJourneyRequired", binding.army_id);
}
// 所有变动前的真实检查cut；玩家只取committed，内部自动采用只取原observed。
FieldControlAcceptanceBoundary capture(const FieldExactContactReadContext &x,
                                       const FieldBattleAbilityReadContext &a,
                                       const FieldBattleSession &s, bool player) {
  if (a.exact_contacts != &x || !a.actual_clock || !a.original_read_boundary ||
      !x.clock.bound_to(x.common, x.military, x.field))
    fail("FieldArmyExitSameWorldSourcesRequired", s.battle_id);
  auto cut = take(capture_field_control_acceptance(x.common, s, *a.actual_clock,
                                                  *a.original_read_boundary, player));
  if (!same_moment(cut.observed_at, take(x.clock.read_current_moment())))
    fail("FieldArmyExitActualMomentRequired", s.battle_id);
  return cut;
}
// 原退出计划按battle+army唯一；当前表不存在与结构损坏不是同一种成功。
const FieldArmyExitPlan *find_plan(const FieldBattleWorldState &world,
                                 const StableId &battle, const StableId &army) {
  const FieldArmyExitPlan *found = nullptr;
  for (const auto &[id, row] : world.army_exit_plans)
    if (row.battle_id == battle && row.army_id == army) {
      if (found || id != row.fact_id) fail("FieldArmyExitChoiceAmbiguous", army);
      found = &row;
    }
  return found;
}
// 军主将的选口权不随战术委派消失；生命与本场真实损失仍按同一个原cut证明。
void require_commander_alive_at_cut(const FieldExactContactReadContext &x,
    const FieldBattleAbilityReadContext &a, const FieldBattleSession &s,
    const FieldFormation &formation, const StableId &actor,
    const FieldControlAcceptanceBoundary &cut) {
  if (!a.original_life || !a.original_life->bound_to(x.common))
    fail("FieldPlayerOriginalLifePrefixRequired", actor);
  if (take(a.original_life->read_actor_death_record_at_original_prefix(
          actor, cut.observed_at, cut.common_prefix)))
    fail("FieldArmyExitLivingCommanderRequired", actor);
  const auto roster = take(read_field_local_roster_at_event_prefix(
      x.common, x.military, x.military_catalog, x.field, s, formation,
      *a.original_rosters, cut.last_local_event_sequence));
  for (const auto *lost : {&roster.dead, &roster.captured, &roster.deserted})
    if (std::find(lost->begin(), lost->end(), actor) != lost->end())
      fail("FieldArmyExitLivingCommanderRequired", actor);
  for (const auto &[id, f] : s.formations) {
    const FieldCommanderCasualty *loss = nullptr;
    for (const auto &row : f.commander_casualties)
      if (row.actor_id == actor && row.loss == 2) {
        if (loss) fail("FieldArmyExitCommanderLossSourceRequired", actor);
        loss = &row;
      }
    if (!loss) continue;
    const FieldBattleEvent *event = nullptr;
    for (const auto &row : s.events)
      if (row.kind == field_event_kind::CommanderLost &&
          row.formation_id == id && same_time(row.at, loss->at)) {
        if (event || row.side != f.side || row.count != 2)
          fail("FieldArmyExitCommanderLossSourceRequired", actor);
        event = &row;
      }
    if (!event) fail("FieldArmyExitCommanderLossSourceRequired", actor);
    if (event->sequence <= cut.last_local_event_sequence)
      fail("FieldArmyExitLivingCommanderRequired", actor);
  }
}
// 同一个选择算法同时服务显式玩家与缺命令自动；实际路线先过滤，再比较全军兵力。
FieldArmyExitPlan prepare(const FieldExactContactReadContext &x,
                         const FieldBattleCatalog &catalog,
                         const FieldBattleAbilityReadContext &a,
                         const MilitaryAccess &original_access,
                         const StableId &battle, const StableId &army,
                         const FieldControlAcceptanceBoundary &cut,
                         const FieldBattleTime &at, const StableId &player,
                         std::optional<std::size_t> explicit_cell,
                         const SourceRef *historical_choice = nullptr) {
  if (a.exact_contacts != &x || original_access.travel_catalog != &x.travel ||
      original_access.common_catalog != &a.common || !a.original_rosters)
    fail("FieldArmyExitSameWorldSourcesRequired", army);
  const auto found = x.field.sessions.find(battle);
  if (found == x.field.sessions.end()) fail("FieldBattleNotFound", battle);
  const auto &s = found->second;
  take(validate_field_control_acceptance(x.common, s, cut));
  const FieldBattleTime *local_boundary = nullptr;
  if (cut.last_local_event_sequence > 0)
    local_boundary = &s.events[std::size_t(cut.last_local_event_sequence - 1)].at;
  else if (s.started_at) local_boundary = &*s.started_at;
  if (!local_boundary || !same_time(*local_boundary, at) ||
      at.tick > cut.observed_at.half_day_tick ||
      __int128_t(cut.observed_at.half_day_tick) - at.tick > 1)
    fail("FieldArmyExitActualChoiceBoundaryRequired", army);
  if ((s.state != FieldBattleState::Running && s.state != FieldBattleState::Ended) ||
      std::tie(at.tick, at.t) > std::tie(s.now.tick, s.now.t))
    fail("FieldArmyExitActualChoiceBoundaryRequired", army);
  const auto *binding = take(read_field_original_battle_binding(x.common, x.field, battle, army));
  const auto now = take(x.clock.read_current_moment());
  const auto command = take(read_military_army_command_source_at_actual_prefix(
      x.common, x.military, army, cut.common_prefix, cut.observed_at, now, &x.formations));
  const ArmyRecord *army_record = nullptr;
  for (const auto &row : x.military.armies) if (row.id == army) {
    if (army_record) fail("FieldArmyExitArmyAmbiguous", army);
    army_record = &row;
  }
  if (!army_record) fail("UnknownArmy", army);
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : x.field.contacts) if (row.battle_id == battle) {
    if (contact) fail("FieldArmyExitOriginalContactRequired", battle);
    contact = &row;
  }
  if (!contact) fail("FieldArmyExitOriginalContactRequired", battle);
  const auto field = take(regenerate_field_contact_battle(x.travel, catalog, contact->source, &x));
  std::optional<Integer> side;
  const FieldFormation *commander_formation = nullptr;
  bool trigger = false;
  for (const auto &event : s.events)
    if (event.sequence <= cut.last_local_event_sequence && event.kind == field_event_kind::Ended)
      trigger = true;
  for (const auto &[id, f] : s.formations) if (f.army_id == army &&
      std::tie(f.joined_at.tick, f.joined_at.t) <= std::tie(at.tick, at.t)) {
    if (f.side < 0 || f.side > 1 || (side && *side != f.side))
      fail("FieldArmyExitOriginalSideRequired", army);
    side = f.side;
    if (f.commander_actor_id == command.commander_actor_id ||
        f.deputy_actor_id == command.commander_actor_id) {
      if (commander_formation) fail("FieldArmyExitCommanderAmbiguous", army);
      commander_formation = &f;
    }
    auto initial = take(read_field_original_initial_placement(x.common, x.military, x.field,
                                                              s, f, field, a));
    if (initial.entry && initial.entry->event_sequence > cut.last_local_event_sequence) continue;
    FieldMission mission = initial.immediate_mission ? *initial.immediate_mission : FieldMission{FieldHold{}};
    bool routing = false;
    for (const auto &event : s.events) {
      if (event.sequence > cut.last_local_event_sequence) break;
      if (event.formation_id != id) continue;
      if (field_event_mission_application(event)) mission = field_event_mission_application(event)->mission;
      if (field_event_mission_transition(event)) mission = field_event_mission_transition(event)->to;
      if (event.kind == field_event_kind::Rout) routing = true;
      if (event.kind == field_event_kind::Rallied) routing = false;
      if (event.kind == field_event_kind::Withdrawn && !routing)
        fail("FieldArmyExitSelectionAfterOrderlyDeparture", army);
    }
    trigger = trigger || (!routing && std::holds_alternative<FieldWithdraw>(mission));
  }
  if (!side || !commander_formation) fail("FieldArmyExitActualCommanderRequired", army);
  if (explicit_cell) {
    if (player != command.commander_actor_id)
      fail("FieldArmyExitCommanderPermissionRequired", player);
    require_commander_alive_at_cut(x, a, s, *commander_formation, player, cut);
  } else if (!trigger) fail("FieldArmyExitActualTriggerRequired", army);
  FieldArmyExitPlan out;
  out.battle_id = battle; out.army_id = army;
  out.commander_actor_id = command.commander_actor_id;
  out.field_hash = field.field_hash; out.selected_at = at;
  out.applied_at = cut.observed_at; out.selection_boundary = cut;
  out.original_journey_opening = original_opening(x, catalog, *binding);
  out.weights = take(read_field_army_exit_weight_prefix(x, catalog, a, battle, army, cut, at));
  // 历史优先来自该军原被接受开程；不从今日returning/归营cache倒填旧选择。
  const auto *accepted = take(read_military_group_order_acceptance(x.common, x.military,
      x.military_catalog, x.carriers, x.movement, x.groups, x.conditions,
      out.original_journey_opening, now, x.author, {}, &x.formations, x.original_finite_returns));
  const CampaignRecord *campaign = nullptr;
  for (const auto &row : x.military.campaigns) if (row.id == accepted->campaign_id) {
    if (campaign) fail("FieldArmyExitCampaignAmbiguous", row.id);
    campaign = &row;
  }
  const MilitaryOrderDefinition *order = nullptr;
  if (campaign) for (const auto &row : x.military_catalog.orders)
    if (row.id == campaign->template_id) {
      if (order) fail("FieldArmyExitCampaignAmbiguous", row.id);
      order = &row;
    }
  const OfficeDefinition *office = nullptr;
  if (order) for (const auto &row : x.military_catalog.offices)
    if (row.id == order->office_id) {
      if (office) fail("FieldArmyExitCampaignAmbiguous", row.id);
      office = &row;
    }
  if (!campaign || !order || !office || campaign->army_id != army ||
      campaign->garrison_id != order->garrison_id ||
      campaign->issuer_organization_id != office->organization_id)
    fail("FieldArmyExitOriginalCampaignRequired", army);
  const auto preferred = accepted->return_garrison_id.value_or("");
  if (!historical_choice && campaign->return_garrison_id &&
      !campaign->return_garrison_id->empty() && *campaign->return_garrison_id != preferred)
    fail("FieldArmyExitPriorReturnChoiceSourceRequired", army);
  if (!x.original_military_access || x.original_military_access->travel_catalog != &x.travel ||
      x.original_military_access->common_catalog != &a.common)
    fail("FieldArmyExitOriginalMilitaryOwnerRequired", army);
  Integer occupation_schema = 1;
  bool occupation_marker = false;
  for (const auto &[name, version] : catalog.identity.module_versions)
    if (name == "military_occupation") {
      if (occupation_marker || version != "2") fail("UnsupportedMilitaryOccupationSchema", army);
      occupation_marker = true; occupation_schema = 2;
    }
  if (x.original_military_access->occupation_source_schema != occupation_schema)
    fail("FieldArmyExitOriginalMilitaryOwnerRequired", army);
  std::optional<SourceRefBuildView> boundary;
  if (historical_choice) boundary = SourceRefBuildView{historical_choice->domain,
      historical_choice->kind, historical_choice->id};
  const auto destination = take(read_military_return_destination_policy(x.military_catalog,
      &a.common, army, order->garrison_id, preferred, {},
      [&](const StableId &garrison) -> Outcome<bool> {
        return guard<bool>([&] {
          const auto owner = take(read_military_garrison_owner_before_actual(x.common,
              x.military, x.military_catalog, *x.original_military_access, garrison,
              boundary, cut.observed_at, now, occupation_schema));
          return owner.organization_id() == office->organization_id;
        });
      },
      [&](const StableId &, const StableId &destination) {
        return read_field_army_return_route_cost(x, catalog, a, battle, army, destination, cut, at);
      }));
  out.destination_settlement_id = destination.destination_settlement_id;
  out.return_garrison_id = destination.return_garrison_id;
  std::vector<std::size_t> eligible;
  for (auto cell : s.sides[*side].exits) {
    auto route = read_field_army_return_route_cost(x, catalog, a, battle, army,
        destination.destination_settlement_id, cut, at, cell);
    if (route.ok) eligible.push_back(cell);
    else {
      const auto &code = route.error.original.code;
      if (route.error.kind != ErrorKind::rule ||
          (code != "NoRoute" && code != "InvalidOrigin" &&
           code != "InvalidDestination" && code != "SegmentBlocked"))
        throw Failure{route.error};
    }
  }
  if (explicit_cell) {
    if (std::find(eligible.begin(), eligible.end(), *explicit_cell) == eligible.end())
      fail("FieldArmyExitReachableDestinationRequired", army);
    out.exit_cell = *explicit_cell;
  } else out.exit_cell = take(read_field_army_weighted_exit_cell(field, out.weights, eligible));
  return out;
}
// 先追加唯一Common头再移动同一准备正文；任何后续失败由整候选撤回。
SourceRef install(CommonStore &common, FieldBattleWorldState &world,
                  FieldArmyExitPlan &&row, CheckpointMemoryCharge &owner) {
  const auto id = take(common.next_owned_id("fact"));
  row.fact_id = id.id();
  owner.grow(0); // 保持原 job 与拥有资源，不再逐型计划。
  take(common.append_fact_borrowed(row.fact_id, "field_army_exit_choice", "military"));
  const SourceRef result{"military", "field_army_exit_choice", row.fact_id};
  StableId key = row.fact_id;
  if (!world.army_exit_plans.emplace(std::move(key), std::move(row)).second)
    fail("FieldArmyExitChoiceAmbiguous", result.id);
  return result;
}
}
// 预览只核当前真正玩家资格与原候选格；不选择、追加来源或移动人产。
Status preview_field_army_exit_select(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const MilitaryAccess &access, const StableId &actor, const FieldArmyExitSelect &request) {
  return status([&] {
    if (x.common.state().metadata.player_actor_id != actor)
      fail("FieldArmyExitPlayerIdentityRequired", actor);
    if (request.exit_cell < 0) fail("FieldArmyExitOriginalExitRequired", request.army_id);
    if (find_plan(x.field, request.battle_id, request.army_id)) fail("FieldArmyExitAlreadyLocked", request.army_id);
    const auto s = x.field.sessions.find(request.battle_id);
    if (s == x.field.sessions.end()) fail("FieldBattleNotFound", request.battle_id);
    auto cut = capture(x, a, s->second, true);
    (void)prepare(x, catalog, a, access, request.battle_id, request.army_id,
                  cut, s->second.now, actor, std::size_t(request.exit_cell));
  });
}
// 显式玩家输入写入真实选择头；Common成功回执尚未封口前不能公开这个候选。
Outcome<FieldBattleMutationResult> apply_field_army_exit_select_in_candidate(
    CommonStore &common, FieldBattleWorldState &world, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const MilitaryAccess &access,
    const StableId &actor, const FieldArmyExitSelect &request, CheckpointMemoryCharge &owner) {
  return guard<FieldBattleMutationResult>([&] {
    if (!a.exact_contacts || &a.exact_contacts->common != &common || &a.exact_contacts->field != &world)
      fail("FieldArmyExitSameWorldSourcesRequired", request.army_id);
    const auto &x = *a.exact_contacts;
    take(preview_field_army_exit_select(x, catalog, a, access, actor, request));
    const auto &s = world.sessions.at(request.battle_id);
    auto cut = capture(x, a, s, true);
    auto plan = prepare(x, catalog, a, access, request.battle_id, request.army_id,
                         cut, s.now, actor, std::size_t(request.exit_cell));
    const auto source = install(common, world, std::move(plan), owner);
    FieldBattleMutationResult out;
    out.battle_id = request.battle_id; out.state = s.state; out.at = s.now;
    out.army_exit_choice_id = source.id; out.selected_exit_cell = request.exit_cell;
    out.actual_control_acceptance = std::move(cut);
    return out;
  });
}
// 自动与未操作玩家走同一源；绝不伪造一条玩家命令结果。
Outcome<SourceRef> ensure_field_army_exit_choice_in_candidate(
    CommonStore &common, FieldBattleWorldState &world, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const MilitaryAccess &access,
    const StableId &battle, const StableId &army, CheckpointMemoryCharge &owner) {
  return guard<SourceRef>([&] {
    if (!a.exact_contacts || &a.exact_contacts->common != &common || &a.exact_contacts->field != &world)
      fail("FieldArmyExitSameWorldSourcesRequired", army);
    const auto &x = *a.exact_contacts;
    if (const auto *old = find_plan(world, battle, army)) {
      const SourceRef source{"military", "field_army_exit_choice", old->fact_id};
      take(read_field_army_exit_choice_source(x, catalog, a, access, source));
      return source;
    }
    const auto found = world.sessions.find(battle);
    if (found == world.sessions.end()) fail("FieldBattleNotFound", battle);
    auto cut = capture(x, a, found->second, false);
    auto row = prepare(x, catalog, a, access, battle, army, cut, found->second.now, {}, {});
    return install(common, world, std::move(row), owner);
  });
}
// 仅把原Common结果的真实唯一回执绑定到其已经写出的选择；不产生另一结果账。
Status bind_field_army_exit_command_result(const CommonStore &common,
    FieldBattleWorldState &world, const StableId &actor, Integer sequence,
    const CommitResult &receipt, CheckpointMemoryCharge &owner) {
  return status([&] {
    if (receipt.kind != "FieldArmyExitSelect") fail("FieldArmyExitCommandResultRequired", receipt.result_id);
    auto original = take(read_common_command_source(common, std::string_view(receipt.result_id)));
    if (original.result != &receipt || original.history->actor_id != actor || original.request_seq != sequence)
      fail("FieldArmyExitCommandResultMismatch", receipt.result_id);
    const auto *payload = receipt.details ? std::get_if<FieldBattleReceipt>(&receipt.details->data) : nullptr;
    if (!payload) fail("FieldArmyExitCommandResultRequired", receipt.result_id);
    take(validate_field_battle_receipt(receipt.kind, *payload));
    const auto &result = std::get<FieldBattleMutationResult>(payload->result);
    const auto found = world.army_exit_plans.find(*result.army_exit_choice_id);
    if (found == world.army_exit_plans.end()) fail("FieldArmyExitOriginalChoiceRequired", receipt.result_id);
    auto &row = found->second;
    const auto session = world.sessions.find(row.battle_id);
    if (session == world.sessions.end()) fail("FieldBattleNotFound", row.battle_id);
    take(validate_field_control_acceptance(common, session->second, *result.actual_control_acceptance));
    bool ended = false;
    for (const auto &event : session->second.events)
      if (event.sequence <= row.selection_boundary.last_local_event_sequence &&
          event.kind == field_event_kind::Ended) ended = true;
    if (receipt.tick != row.applied_at.half_day_tick ||
        result.state != (ended ? FieldBattleState::Ended : FieldBattleState::Running))
      fail("FieldArmyExitCommandResultMismatch", receipt.result_id);
    if (row.player_command_result || row.commander_actor_id != actor || row.battle_id != result.battle_id ||
        row.exit_cell != std::size_t(*result.selected_exit_cell) || !same_time(row.selected_at, result.at) ||
        row.selection_boundary.common_prefix.common_fact_count != result.actual_control_acceptance->common_prefix.common_fact_count ||
        row.selection_boundary.last_local_event_sequence != result.actual_control_acceptance->last_local_event_sequence ||
        !same_moment(row.applied_at, result.actual_control_acceptance->observed_at))
      fail("FieldArmyExitCommandResultMismatch", receipt.result_id);
    owner.grow(0); // 保持原 job 检查；实际 SourceRef 仍普通复制。
    row.player_command_result = SourceRef{"common", "command_result", checkpoint_fresh_string_copy(receipt.result_id)};
  });
}
// 选择头紧接原检查cut；本读口只验证选择，不把尚未存在的汇合/开程说成完成。
Outcome<const FieldArmyExitPlan *> read_field_army_exit_choice_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const MilitaryAccess &access,
    const SourceRef &source) {
  return guard<const FieldArmyExitPlan *>([&] {
    if (source.domain != "military" || source.kind != "field_army_exit_choice")
      fail("FieldArmyExitOriginalChoiceRequired", source.id);
    const auto found = x.field.army_exit_plans.find(source.id);
    if (found == x.field.army_exit_plans.end() || found->second.fact_id != source.id)
      fail("FieldArmyExitOriginalChoiceRequired", source.id);
    const auto &r = found->second;
    if (find_plan(x.field, r.battle_id, r.army_id) != &r)
      fail("FieldArmyExitChoiceAmbiguous", r.army_id);
    const auto position = take(read_actual_common_fact_position(x.common, source));
    const auto session = x.field.sessions.find(r.battle_id);
    if (session == x.field.sessions.end()) fail("FieldBattleNotFound", r.battle_id);
    const auto &s = session->second;
    take(validate_field_control_acceptance(x.common, s, r.selection_boundary));
    if (r.selection_boundary.common_prefix.common_fact_count < 0 ||
        position != std::size_t(r.selection_boundary.common_prefix.common_fact_count) ||
        !same_moment(r.applied_at, r.selection_boundary.observed_at) ||
        take(compare_world_moments(r.applied_at, take(x.clock.read_current_moment()))) > 0)
      fail("FieldArmyExitChoiceActualCutMismatch", r.fact_id);
    StableId player;
    if (r.player_command_result) {
      auto actual = take(read_common_command_source(x.common, *r.player_command_result));
      const auto *details = actual.result->details ?
          std::get_if<FieldBattleReceipt>(&actual.result->details->data) : nullptr;
      if (!details || actual.result->kind != "FieldArmyExitSelect" ||
          actual.history->actor_id != r.commander_actor_id)
        fail("FieldArmyExitCommandResultMismatch", r.fact_id);
      take(validate_field_battle_receipt(actual.result->kind, *details));
      const auto &result = std::get<FieldBattleMutationResult>(details->result);
      take(validate_field_control_acceptance(x.common, s, *result.actual_control_acceptance));
      bool ended = false;
      for (const auto &event : s.events)
        if (event.sequence <= r.selection_boundary.last_local_event_sequence &&
            event.kind == field_event_kind::Ended) ended = true;
      if (actual.result->tick != r.applied_at.half_day_tick ||
          result.state != (ended ? FieldBattleState::Ended : FieldBattleState::Running))
        fail("FieldArmyExitCommandResultMismatch", r.fact_id);
      if (result.battle_id != r.battle_id || result.army_exit_choice_id != r.fact_id ||
          *result.selected_exit_cell < 0 || std::size_t(*result.selected_exit_cell) != r.exit_cell ||
          !same_time(result.at, r.selected_at) ||
          result.actual_control_acceptance->common_prefix.common_fact_count !=
              r.selection_boundary.common_prefix.common_fact_count ||
          result.actual_control_acceptance->last_local_event_sequence !=
              r.selection_boundary.last_local_event_sequence ||
          !same_moment(result.actual_control_acceptance->observed_at, r.applied_at))
        fail("FieldArmyExitCommandResultMismatch", r.fact_id);
      player = actual.history->actor_id;
    }
    const auto expected = prepare(x, catalog, a, access, r.battle_id, r.army_id,
        r.selection_boundary, r.selected_at, player,
        r.player_command_result ? std::optional<std::size_t>{r.exit_cell} : std::nullopt, &source);
    if (expected.commander_actor_id != r.commander_actor_id ||
        expected.field_hash != r.field_hash || expected.exit_cell != r.exit_cell ||
        expected.destination_settlement_id != r.destination_settlement_id ||
        expected.return_garrison_id != r.return_garrison_id ||
        !same_source(expected.original_journey_opening, r.original_journey_opening) ||
        expected.weights.size() != r.weights.size())
      fail("FieldArmyExitChoiceSourceMismatch", r.fact_id);
    for (std::size_t i = 0; i < r.weights.size(); ++i) {
      const auto &left = expected.weights[i], &right = r.weights[i];
      if (left.formation_id != right.formation_id || left.fit != right.fit ||
          left.wounded != right.wounded || left.position_dm.x != right.position_dm.x ||
          left.position_dm.y != right.position_dm.y)
        fail("FieldArmyExitChoiceWeightMismatch", r.fact_id);
    }
    return &r;
  });
}
// 仅验证持久双向绑定；完整权限、路线和权重由上方原选择读口继续验证。
Status validate_field_army_exit_player_command_source(const CommonStore &common,
    const FieldBattleWorldState &world, const FieldArmyExitPlan &row) {
  return status([&] {
    const auto plan = world.army_exit_plans.find(row.fact_id);
    if (plan == world.army_exit_plans.end() || &plan->second != &row ||
        find_plan(world, row.battle_id, row.army_id) != &row)
      fail("FieldArmyExitOriginalChoiceRequired", row.fact_id);
    const auto session = world.sessions.find(row.battle_id);
    if (session == world.sessions.end()) fail("FieldBattleNotFound", row.battle_id);
    take(validate_field_control_acceptance(common, session->second, row.selection_boundary));
    const auto position = take(read_actual_common_fact_position(
        common, SourceRef{"military", "field_army_exit_choice", row.fact_id}));
    if (row.selection_boundary.common_prefix.common_fact_count < 0 ||
        position != std::size_t(row.selection_boundary.common_prefix.common_fact_count) ||
        !same_moment(row.applied_at, row.selection_boundary.observed_at))
      fail("FieldArmyExitChoiceActualCutMismatch", row.fact_id);
    if (!row.player_command_result) return;
    const auto source = take(read_common_command_source(common, *row.player_command_result));
    const auto &receipt = *source.result;
    const auto *payload = receipt.details ?
        std::get_if<FieldBattleReceipt>(&receipt.details->data) : nullptr;
    if (receipt.kind != "FieldArmyExitSelect" || !payload ||
        source.history->actor_id != row.commander_actor_id)
      fail("FieldArmyExitCommandResultMismatch", row.fact_id);
    take(validate_field_battle_receipt(receipt.kind, *payload));
    const auto &result = std::get<FieldBattleMutationResult>(payload->result);
    take(validate_field_control_acceptance(common, session->second, *result.actual_control_acceptance));
    bool ended = false;
    for (const auto &event : session->second.events)
      if (event.sequence <= row.selection_boundary.last_local_event_sequence &&
          event.kind == field_event_kind::Ended) ended = true;
    if (receipt.tick != row.applied_at.half_day_tick ||
        result.state != (ended ? FieldBattleState::Ended : FieldBattleState::Running) ||
        result.battle_id != row.battle_id || result.army_exit_choice_id != row.fact_id ||
        *result.selected_exit_cell < 0 || std::size_t(*result.selected_exit_cell) != row.exit_cell ||
        !same_time(result.at, row.selected_at) ||
        result.actual_control_acceptance->common_prefix.common_fact_count !=
            row.selection_boundary.common_prefix.common_fact_count ||
        result.actual_control_acceptance->last_local_event_sequence !=
            row.selection_boundary.last_local_event_sequence ||
        !same_moment(result.actual_control_acceptance->observed_at, row.applied_at))
      fail("FieldArmyExitCommandResultMismatch", row.fact_id);
  });
}
// 每张真实选择表都查原source，每个同kind Common头也必须反向命中唯一正文。
Status validate_field_army_exit_choice_sources(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const MilitaryAccess &access) {
  return status([&] {
    for (const auto &[id, row] : x.field.army_exit_plans) {
      if (id != row.fact_id) fail("FieldArmyExitOriginalChoiceRequired", id);
      take(read_field_army_exit_choice_source(x, catalog, a, access,
          SourceRef{"military", "field_army_exit_choice", id}));
    }
    for (const auto &[id, head] : x.common.state().facts.rows())
      if (head.kind == "field_army_exit_choice") {
        const auto row = x.field.army_exit_plans.find(id);
        if (head.domain != "military" || row == x.field.army_exit_plans.end() ||
            row->second.fact_id != id)
          fail("FieldArmyExitOriginalChoiceRequired", id);
      }
  });
}
// 有序路径只用已经锁定的一军单口；Routing从不调用本投影。
Outcome<PointI> read_field_locked_army_exit_target(const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    const GeneratedBattleField &field, const FieldBattleTime &at, Integer sequence) {
  return guard<PointI>([&] {
    const auto actual_session = world.sessions.find(session.battle_id);
    const auto actual_formation = session.formations.find(formation.formation_id);
    if (actual_session == world.sessions.end() || &actual_session->second != &session ||
        actual_formation == session.formations.end() || &actual_formation->second != &formation)
      fail("FieldArmyExitSameWorldSourcesRequired", formation.army_id);
    const auto *plan = find_plan(world, session.battle_id, formation.army_id);
    if (!plan || plan->field_hash != field.field_hash || session.field_hash != field.field_hash ||
        plan->selection_boundary.last_local_event_sequence > sequence ||
        std::tie(plan->selected_at.tick, plan->selected_at.t) > std::tie(at.tick, at.t) ||
        formation.side < 0 || formation.side > 1 || plan->exit_cell >= field.cells.size() ||
        std::find(session.sides[formation.side].exits.begin(),
                  session.sides[formation.side].exits.end(), plan->exit_cell) ==
            session.sides[formation.side].exits.end())
      fail("FieldArmyExitLockedSourceRequired", formation.army_id);
    return field_cell_center_dm(field, plan->exit_cell);
  });
}
// 军主将提前显式选口与自动选择共享一张源；胜方未离场者仍由原Opening接续。
Status ensure_field_required_army_exit_choices_in_candidate(CommonStore &common,
    FieldBattleWorldState &world, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const StableId &battle, bool terminal,
    CheckpointMemoryCharge &owner) {
  return status([&] {
    if (!a.exact_contacts || !a.exact_contacts->original_military_access ||
        &a.exact_contacts->common != &common || &a.exact_contacts->field != &world)
      fail("FieldArmyExitOriginalMilitaryOwnerRequired", battle);
    const auto found = world.sessions.find(battle);
    if (found == world.sessions.end()) fail("FieldBattleNotFound", battle);
    const auto &s = found->second;
    if (terminal != (s.state == FieldBattleState::Ended) || (terminal && !s.result))
      fail("FieldArmyExitActualTriggerRequired", battle);
    std::set<StableId> armies;
    for (const auto &[id, f] : s.formations) {
      if (f.side < 0 || f.side > 1) fail("FieldArmyExitOriginalSideRequired", f.army_id);
      if (terminal) {
        if (take(read_field_army_terminal_exit_required(s, f.army_id)))
          armies.insert(f.army_id);
      } else if (!f.fit.empty() && f.state != FieldFormationState::Routing &&
                 f.state != FieldFormationState::Withdrawn &&
                 f.state != FieldFormationState::Destroyed &&
                 std::holds_alternative<FieldWithdraw>(f.mission))
        armies.insert(f.army_id);
    }
    for (const auto &army : armies)
      take(ensure_field_army_exit_choice_in_candidate(common, world, catalog, a,
          *a.exact_contacts->original_military_access, battle, army, owner));
  });
}
// 本读口只判原终局后谁需要离场，不授军令、路线或当前位置权限。
Outcome<bool> read_field_army_terminal_exit_required(const FieldBattleSession &s,
                                                     const StableId &army) {
  return guard<bool>([&] {
    if (s.state != FieldBattleState::Ended || !s.result)
      fail("FieldArmyExitActualTerminalRequired", army);
    bool present = false, survivors = false, actual_withdrawal = false;
    std::optional<Integer> side;
    for (const auto &[id, f] : s.formations) {
      if (f.army_id != army) continue;
      present = true;
      if (f.side < 0 || f.side > 1 || (side && *side != f.side))
        fail("FieldArmyExitOriginalSideRequired", army);
      side = f.side;
      const auto retained = [&](const StableId &actor) {
        if (actor.empty()) return false;
        for (const auto *lost : {&f.dead, &f.captured, &f.deserted})
          if (std::find(lost->begin(), lost->end(), actor) != lost->end()) return false;
        for (const auto &loss : f.commander_casualties)
          if (loss.actor_id == actor && loss.loss == 2) return false;
        return true;
      };
      survivors = survivors || !f.fit.empty() || !f.wounded.empty() ||
          retained(f.commander_actor_id) || (f.deputy_actor_id && retained(*f.deputy_actor_id));
      bool routing = false;
      for (const auto &event : s.events) {
        if (event.formation_id != id) continue;
        if (event.kind == field_event_kind::Rout) routing = true;
        if (event.kind == field_event_kind::Rallied) routing = false;
        if (event.kind == field_event_kind::Withdrawn && !routing) actual_withdrawal = true;
      }
      actual_withdrawal = actual_withdrawal ||
          (!routing && std::holds_alternative<FieldWithdraw>(f.mission));
    }
    if (!present || !side) fail("FieldArmyExitActualFormationRequired", army);
    return survivors && (*side != s.result->winner_side || actual_withdrawal);
  });
}
} // namespace hanmo::resident
