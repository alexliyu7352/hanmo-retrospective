#include "field_event_legacy_kinds.hpp"
#include "field_army_exit_rally.hpp"
#include "field_ability_sources.hpp"
#include "field_author_data.hpp"
#include "field_binding_source.hpp"
#include "field_contact_origin.hpp"
#include "field_departure_source.hpp"
#include "field_exact_contact_source.hpp"
#include "field_roster_origin.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_settlement_exit_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 普通兵及将领名单均按原稳定actor ID唯一化，不复制任何Army manifest或财物。
void canonical_members(std::vector<StableId> &actors, const StableId &id) {
  std::sort(actors.begin(), actors.end());
  if (std::adjacent_find(actors.begin(), actors.end()) != actors.end())
    fail("FieldArmyExitRallyMemberAmbiguous", id);
}
// 原Withdrawn之前的真实Rout/Rallied决定谁是普通逃散，不能用今日Withdrawn标签猜。
bool routing_exit(const FieldBattleSession &s, const FieldFormation &f) {
  bool routing = false;
  for (const auto &event : s.events) {
    if (event.formation_id != f.formation_id) continue;
    if (event.kind == field_event_kind::Rout) routing = true;
    if (event.kind == field_event_kind::Rallied) routing = false;
    if (event.kind == field_event_kind::Withdrawn) return routing;
  }
  return false;
}
// 冷读正文精确比较，不允许删一个尾员、把普通兵冒将领或改写已退出来源。
bool same_origin(const FieldArmyExitRallyFormation &a, const FieldArmyExitRallyFormation &b) {
  return a.formation_id == b.formation_id &&
      a.initial_position_dm.x == b.initial_position_dm.x &&
      a.initial_position_dm.y == b.initial_position_dm.y &&
      a.ordinary_member_ids == b.ordinary_member_ids &&
      a.retained_officer_ids == b.retained_officer_ids &&
      bool(a.original_exit) == bool(b.original_exit) &&
      (!a.original_exit || same_source(*a.original_exit, *b.original_exit));
}
}
// 读取真正终局后的名册：原battle结果仍冻结；此处不把旧formation搬回战场。
Outcome<FieldArmyExitRallyOriginRead> read_field_army_exit_rally_origin(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const FieldArmyExitPlan &plan,
    const SourceRef &terminal_segment) {
  return guard<FieldArmyExitRallyOriginRead>([&] {
    if (!x.original_military_access || a.exact_contacts != &x || !a.original_rosters ||
        !x.clock.bound_to(x.common, x.military, x.field))
      fail("FieldArmyExitSameWorldSourcesRequired", plan.army_id);
    const auto *choice = take(read_field_army_exit_choice_source(x, catalog, a,
        *x.original_military_access, {"military", "field_army_exit_choice", plan.fact_id}));
    if (choice != &plan) fail("FieldArmyExitOriginalChoiceRequired", plan.fact_id);
    const auto terminal = take(read_field_segment_settlement_source(
        x.common, x.military, x.field, terminal_segment, &x.clock));
    if (!terminal.session || !terminal.settlement || !terminal.terminal ||
        terminal.session->battle_id != plan.battle_id ||
        terminal.session->state != FieldBattleState::Ended || !terminal.session->result ||
        terminal.settlement != &terminal.session->settlements.back())
      fail("FieldArmyExitActualTerminalRequired", plan.army_id);
    const auto &s = *terminal.session;
    if (!take(read_field_army_terminal_exit_required(s, plan.army_id)))
      fail("FieldArmyExitActualWithdrawalRequired", plan.army_id);
    const auto &ended = s.result->ended;
    if (s.next_event_sequence <= 0 || s.now.tick != ended.tick || s.now.t != ended.t ||
        terminal.settlement->settled_at.tick != ended.tick ||
        terminal.settlement->settled_at.t != ended.t)
      fail("FieldArmyExitFrozenTerminalRequired", plan.army_id);
    if (take(read_actual_common_fact_position(x.common,
             SourceRef{"military", "field_army_exit_choice", plan.fact_id})) >=
        take(read_actual_common_fact_position(x.common, terminal_segment)))
      fail("FieldArmyExitChoiceBeforeTerminalRequired", plan.army_id);
    const auto *binding = take(read_field_original_battle_binding(
        x.common, x.field, plan.battle_id, plan.army_id));
    if (const auto *boundary = std::get_if<FieldBoundaryContactBinding>(&binding->source)) {
      const auto queue = take(read_field_original_reinforcement_queue(
          x, catalog, *a.original_rosters,
          SourceRef{"military", "field_battle_contact", boundary->contact_id}));
      if (queue->admitted.size() != queue->formation_ids.size())
        fail("FieldArmyExitUnadmittedMemberRallyRequired", boundary->contact_id);
      for (const auto &id : queue->formation_ids)
        if (!s.formations.contains(id))
          fail("FieldArmyExitUnadmittedMemberRallyRequired", id);
    }
    const FieldBattleContactRecord *contact = nullptr;
    for (const auto &[id, row] : x.field.contacts) if (row.battle_id == plan.battle_id) {
      if (contact) fail("FieldArmyExitOriginalContactRequired", plan.battle_id);
      contact = &row;
    }
    if (!contact) fail("FieldArmyExitOriginalContactRequired", plan.battle_id);
    const auto field = take(regenerate_field_contact_battle(x.travel, catalog, contact->source, &x));
    if (field.field_hash != plan.field_hash || plan.exit_cell >= field.cells.size())
      fail("FieldArmyExitOriginalGeometryRequired", plan.army_id);
    const auto target = field_cell_center_dm(field, plan.exit_cell);
    FieldArmyExitRallyOriginRead out;
    out.terminal = terminal.settlement;
    // 当前窗口中途终局的下一完整半日末；恰好窗末终局则下一窗末。
    const auto windows = ended.t == 0 ? Integer{1} : Integer{2};
    if (ended.tick > std::numeric_limits<Integer>::max() - windows)
      fail("FieldBattleTimeOverflow", plan.army_id);
    out.ordinary_deadline = {ended.tick + windows, 0};
    std::set<StableId> all_members;
    for (const auto &[id, f] : s.formations) {
      if (f.army_id != plan.army_id) continue;
      const auto roster = take(read_field_local_roster_at_event_prefix(
          x.common, x.military, x.military_catalog, x.field, s, f,
          *a.original_rosters, s.next_event_sequence - 1));
      FieldArmyExitRallyFormation row;
      row.formation_id = id; row.initial_position_dm = f.position_dm;
      const auto is_officer = [&](const StableId &actor) {
        return actor == f.commander_actor_id || f.deputy_actor_id == actor;
      };
      const auto lost = [&](const StableId &actor) {
        for (const auto *group : {&roster.dead, &roster.captured, &roster.deserted})
          if (std::find(group->begin(), group->end(), actor) != group->end()) return true;
        for (const auto &loss : f.commander_casualties)
          if (loss.actor_id == actor && loss.loss == 2) return true;
        return false;
      };
      for (const auto *group : {&roster.fit, &roster.wounded})
        for (const auto &actor : *group)
          if (!is_officer(actor)) row.ordinary_member_ids.push_back(actor);
      const auto add_officer = [&](const StableId &actor) {
        if (actor.empty() || lost(actor)) return;
        const auto count = std::count_if(binding->members.begin(), binding->members.end(),
            [&](const auto &member) { return member.actor_id == actor && member.formation_id == id; });
        if (count != 1 || !x.common.actor(actor))
          fail("FieldArmyExitOriginalOfficerRequired", actor);
        row.retained_officer_ids.push_back(actor);
      };
      add_officer(f.commander_actor_id);
      if (f.deputy_actor_id) add_officer(*f.deputy_actor_id);
      canonical_members(row.ordinary_member_ids, id);
      canonical_members(row.retained_officer_ids, id);
      for (const auto *group : {&row.ordinary_member_ids, &row.retained_officer_ids})
        for (const auto &actor : *group) {
          if (!all_members.insert(actor).second) fail("FieldArmyExitRallyMemberAmbiguous", actor);
          const auto count = std::count_if(binding->members.begin(), binding->members.end(),
              [&](const auto &member) { return member.actor_id == actor && member.formation_id == id; });
          // 原军籍ID包括非Common普通兵；只有真具名人物属于S1 actor binding。
          if ((x.common.actor(actor) && count != 1) || (!x.common.actor(actor) && count != 0))
            fail("FieldArmyExitOriginalMemberRequired", actor);
        }
      if (row.ordinary_member_ids.empty() && row.retained_officer_ids.empty()) continue;
      if (f.state == FieldFormationState::Withdrawn && !routing_exit(s, f)) {
        const FieldBattleExitRecord *exit = nullptr;
        for (const auto &[key, old] : x.field.exits)
          if (old.battle_id == plan.battle_id && old.formation_id == id) {
            if (exit || key != old.fact_id) fail("FieldArmyExitOriginalExitAmbiguous", id);
            exit = &old;
          }
        if (!exit) fail("FieldArmyExitOriginalExitRequired", id);
        const SourceRef source{"military", "field_battle_exit", exit->fact_id};
        const auto actual = take(read_field_actual_exit_application(x.common, x.military,
            x.field, x.travel, catalog, source, x.clock, {}, &x, a.diplomacy, a.territory));
        if (actual != exit || exit->field_point_dm.x != target.x || exit->field_point_dm.y != target.y)
          fail("FieldArmyExitLockedOrderlyPointRequired", id);
        row.original_exit = source;
      }
      out.formations.push_back(std::move(row));
    }
    // 真实未入场余队仍在原contact；本读口不伪造Entered或把他们投影到任何出口。
    for (const auto &member : binding->members)
      if (!s.formations.contains(member.formation_id))
        fail("FieldArmyExitUnadmittedMemberRallyRequired", member.formation_id);
    if (out.formations.empty()) fail("FieldArmyExitReturningMemberRequired", plan.army_id);
    return out;
  });
}
// 初始化只接受当前原terminal采用；重复调用严格比对原来源，不重置已走出的步骤。
Status begin_field_army_exit_rallies_in_candidate(CommonStore &common,
    FieldBattleWorldState &world, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &terminal_segment,
    CheckpointMemoryCharge &owner) {
  return status([&] {
    if (!a.exact_contacts || &a.exact_contacts->common != &common ||
        &a.exact_contacts->field != &world) fail("FieldArmyExitSameWorldSourcesRequired", terminal_segment.id);
    const auto &x = *a.exact_contacts;
    const auto terminal = take(read_field_segment_settlement_source(
        common, x.military, world, terminal_segment, &x.clock));
    if (!terminal.terminal || !terminal.settlement ||
        !same_moment(terminal.settlement->applied_at, take(x.clock.read_current_moment())))
      fail("FieldArmyExitActualTerminalRequired", terminal_segment.id);
    for (auto &[id, plan] : world.army_exit_plans) {
      if (plan.battle_id != terminal.session->battle_id) continue;
      if (!take(read_field_army_terminal_exit_required(*terminal.session, plan.army_id))) {
        if (plan.terminal_segment || plan.ordinary_deadline || !plan.rally_formations.empty() ||
            !plan.steps.empty() || plan.ready_source || plan.ready_at ||
            plan.ready_applied_at || plan.actual_continuation)
          fail("FieldArmyExitUnexpectedRallySource", id);
        continue;
      }
      auto actual = take(read_field_army_exit_rally_origin(x, catalog, a, plan, terminal_segment));
      if (plan.terminal_segment) {
        if (!same_source(*plan.terminal_segment, terminal_segment) || !plan.ordinary_deadline ||
            plan.ordinary_deadline->tick != actual.ordinary_deadline.tick ||
            plan.ordinary_deadline->t != actual.ordinary_deadline.t ||
            plan.rally_formations.size() != actual.formations.size())
          fail("FieldArmyExitRallyOriginMismatch", id);
        for (std::size_t n = 0; n < actual.formations.size(); ++n)
          if (!same_origin(plan.rally_formations[n], actual.formations[n]))
            fail("FieldArmyExitRallyOriginMismatch", id);
        continue;
      }
      if (plan.ordinary_deadline || !plan.rally_formations.empty() || !plan.steps.empty() ||
          plan.ready_source || plan.ready_at || plan.ready_applied_at || plan.actual_continuation)
        fail("FieldArmyExitRallyOriginMismatch", id);
      owner.grow(0); // 原来源普通复制/移动，仍保同 job 归属。
      plan.terminal_segment = terminal_segment;
      plan.ordinary_deadline = actual.ordinary_deadline;
      plan.rally_formations = std::move(actual.formations);
    }
  });
}
} // namespace hanmo::resident
