#include "field_army_exit_ready.hpp"
#include "field_ability_sources.hpp"
#include "field_army_exit_placement.hpp"
#include "field_author_data.hpp"
#include "field_binding_source.hpp"
#include "field_boundary_source.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_roster_origin.hpp"
#include "resident/domains/military/training_exact.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
bool equal_time(const FieldBattleTime &l, const FieldBattleTime &r) { return l.tick == r.tick && l.t == r.t; }
bool equal_point(PointI l, PointI r) { return l.x == r.x && l.y == r.y; }
// ready沿本实际窗口closing，不把局部秒转换为一个新世界Moment。
void require_ready_closing(const FieldExactContactReadContext &x,
    const FieldArmyExitPlan &p, std::size_t position) {
  const auto &at = *p.ready_applied_at;
  const auto now = take(x.clock.read_current_moment());
  if (!x.periods || at.half_day_tick <= std::numeric_limits<Integer>::min() ||
      __int128_t(p.ready_at->tick) + (p.ready_at->t ? 1 : 0) != at.half_day_tick ||
      take(compare_fractions(at.sub_half_day, CanonicalFraction{})) != 0 ||
      take(compare_world_moments(at, now)) > 0)
    fail("FieldArmyExitReadyActualWindowRequired", p.fact_id);
  const auto period = take(x.periods->read_training_period(at.half_day_tick - 1));
  if (period.from_tick != at.half_day_tick - 1 || period.to_tick != at.half_day_tick ||
      period.original_opening.source_schema != 1 ||
      take(read_actual_common_fact_position(x.common, period.original_opening.source_ref)) >= position)
    fail("FieldArmyExitOriginalWindowRequired", p.fact_id);
  if (same_moment(at, now)) {
    if (!same_moment(at, take(x.clock.read_field_window_closing_moment())))
      fail("FieldArmyExitReadyActualWindowRequired", p.fact_id);
  } else {
    const auto after = take(x.periods->read_training_period(at.half_day_tick));
    if (!same_moment(after.opening_at, at) || after.original_opening.source_schema != 1 ||
        take(read_actual_common_fact_position(x.common, after.original_opening.source_ref)) <= position)
      fail("FieldArmyExitOriginalWindowRequired", p.fact_id);
  }
}
// 原初始双军/后来单军各取自己的真实prefix和已付close，不能借另一端缴费行。
void original_march(const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
                    FieldArmyExitReadyRead &out, ExactArithmeticBudget b) {
  const auto &binding = *out.original_binding;
  if (const auto *initial = std::get_if<FieldInitialContactBinding>(&binding.source)) {
    const auto contact = x.field.contacts.find(initial->contact_id);
    const auto *origin = contact == x.field.contacts.end() ? nullptr :
        std::get_if<FieldExactGroupContactOrigin>(&contact->second.source);
    if (!origin) fail("FieldArmyExitExactContactRequired", binding.army_id);
    const auto actual = take(read_field_exact_road_contact_source(x, origin->actual_contact, catalog.parameters, b));
    for (const auto &end : actual.endpoints) if (end.army && end.army->id == binding.army_id) {
      if (out.original_journey) fail("FieldArmyExitOriginalJourneyRequired", binding.army_id);
      out.original_journey = end.journey; out.original_opening = end.opening;
      out.original_close = end.closed; out.original_contact = origin->actual_contact;
    }
  } else {
    const auto &id = std::get<FieldBoundaryContactBinding>(binding.source).contact_id;
    const auto contact = x.field.boundary_contacts.find(id);
    const auto *origin = contact == x.field.boundary_contacts.end() ? nullptr :
        std::get_if<FieldExactBoundaryOrigin>(&contact->second.motion);
    if (!origin) fail("FieldArmyExitExactBoundaryRequired", binding.army_id);
    const auto actual = take(read_field_boundary_original_source(x, catalog, origin->actual_contact, b));
    if (!actual.army || actual.army->id != binding.army_id)
      fail("FieldArmyExitOriginalJourneyRequired", binding.army_id);
    out.original_journey = actual.journey; out.original_opening = actual.opening;
    out.original_close = actual.closed; out.original_contact = origin->actual_contact;
  }
  if (!out.original_journey || !out.original_opening || !out.original_close ||
      !same_source(out.plan->original_journey_opening,
                   {"travel", "group_movement_opening", out.original_opening->id}))
    fail("FieldArmyExitOriginalJourneyRequired", binding.army_id);
  out.original_prefix = x.movement.prefixes.find(out.original_journey->movement_prefix_ref);
  if (!out.original_prefix || out.original_prefix->id != out.original_close->prefix_id ||
      !same_source(out.original_close->actual_cause, out.original_contact))
    fail("FieldArmyExitOriginalClosedPrefixRequired", binding.army_id);
}
}
// read只证明原ready，不调用group全表反覆盖，避免后继ready release→ready原源的循环。
Outcome<FieldArmyExitReadyRead> read_field_army_exit_ready_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &source, ExactArithmeticBudget b) {
  return guard<FieldArmyExitReadyRead>([&] {
    if (source.domain != "military" || source.kind != "field_army_exit_ready" ||
        a.exact_contacts != &x || !x.clock.bound_to(x.common, x.military, x.field) ||
        !a.original_rosters || &a.original_rosters->common != &x.common ||
        &a.original_rosters->military != &x.military || x.periods != &a.original_rosters->periods)
      fail("FieldArmyExitReadySourceRequired", source.id);
    const auto position = take(read_actual_common_fact_position(x.common, source));
    FieldArmyExitReadyRead out;
    for (const auto &[id, p] : x.field.army_exit_plans)
      if (p.ready_source && same_source(*p.ready_source, source)) {
        if (out.plan) fail("FieldArmyExitReadySourceAmbiguous", source.id);
        out.plan = &p;
      }
    if (!out.plan || !out.plan->terminal_segment || !out.plan->ready_at || !out.plan->ready_applied_at)
      fail("FieldArmyExitReadySourceRequired", source.id);
    const auto &p = *out.plan;
    if (take(read_actual_common_fact_position(x.common, *p.terminal_segment)) >= position)
      fail("FieldArmyExitReadySourceOrderRequired", source.id);
    for (const auto &step : p.steps)
      if (take(read_actual_common_fact_position(x.common,
              SourceRef{"military", "field_army_exit_step", step.fact_id})) >= position)
        fail("FieldArmyExitReadySourceOrderRequired", source.id);
    const auto current = take(read_field_army_exit_progress(x, catalog, a,
        {"military", "field_army_exit_choice", p.fact_id}));
    if (!equal_time(current.at, *p.ready_at) ||
        take(read_actual_common_fact_position(x.common, *p.terminal_segment)) >= position)
      fail("FieldArmyExitReadySourceOrderRequired", source.id);
    for (const auto &step : p.steps)
      if (take(read_actual_common_fact_position(x.common,
              SourceRef{"military", "field_army_exit_step", step.fact_id})) >= position)
        fail("FieldArmyExitReadySourceOrderRequired", source.id);
    require_ready_closing(x, p, position);
    const auto session = x.field.sessions.find(p.battle_id);
    if (session == x.field.sessions.end()) fail("FieldBattleNotFound", p.battle_id);
    out.session = &session->second;
    out.original_binding = take(read_field_original_battle_binding(x.common, x.field, p.battle_id, p.army_id));
    original_march(x, catalog, out, b);
    const FieldBattleContactRecord *contact = nullptr;
    for (const auto &[id, row] : x.field.contacts) if (row.battle_id == p.battle_id) {
      if (contact) fail("FieldArmyExitOriginalContactRequired", p.battle_id);
      contact = &row;
    }
    if (!contact) fail("FieldArmyExitOriginalContactRequired", p.battle_id);
    const auto field = take(regenerate_field_contact_battle(x.travel, catalog, contact->source, &x, b));
    if (p.exit_cell >= field.cells.size() || p.field_hash != field.field_hash)
      fail("FieldArmyExitOriginalGeometryRequired", p.fact_id);
    const auto target = field_cell_center_dm(field, p.exit_cell);
    std::set<StableId> members;
    std::size_t survivors = 0;
    std::optional<Integer> side;
    for (const auto &f : current.formations) {
      const auto formation = out.session->formations.find(f.origin->formation_id);
      if (formation == out.session->formations.end() || formation->second.army_id != p.army_id ||
          (side && *side != formation->second.side)) fail("FieldArmyExitActualFormationRequired", p.army_id);
      side = formation->second.side;
      const auto count = f.ordinary_member_ids.size() + f.origin->retained_officer_ids.size();
      survivors += count;
      if (count && !f.origin->original_exit && !equal_point(f.position_dm, target))
        fail("FieldArmyExitReadyAllMembersRequired", p.army_id);
      const GroupContactMemberReleaseSource *prior = nullptr;
      if (f.origin->original_exit) {
        for (const auto &[id, release] : x.groups.contact_member_releases.rows())
          if (same_source(release.actual_release_source, *f.origin->original_exit)) {
            if (prior || release.journey_id != out.original_journey->journey_id ||
                !same_source(release.original_contact, out.original_contact) ||
                take(read_actual_common_fact_position(x.common,
                    SourceRef{"travel", "group_contact_member_release", id})) >= position)
              fail("FieldArmyExitOriginalMemberReleaseRequired", id);
            prior = &release;
          }
      }
      for (const auto *ids : {&f.ordinary_member_ids, &f.origin->retained_officer_ids})
        for (const auto &actor : *ids) {
          if (!members.insert(actor).second) fail("FieldArmyExitRallyMemberAmbiguous", actor);
          if (!x.common.actor(actor)) continue;
          out.named_participants.push_back(actor);
          if (f.origin->original_exit) {
            if (!prior || std::count(prior->named_participants.begin(), prior->named_participants.end(), actor) != 1)
              fail("FieldArmyExitOriginalMemberReleaseRequired", actor);
          } else out.held_named_participants.push_back(actor);
        }
      if (prior) {
        if (!x.original_member_releases ||
            !x.original_member_releases->bound_to(x.common, x.movement, x.groups) ||
            (x.group_context && x.group_context->member_releases != x.original_member_releases))
          fail("FieldArmyExitOriginalMemberReleaseOwnerRequired", p.army_id);
        take(x.original_member_releases->validate_actual_group_member_release(*prior));
        out.prior_orderly_releases.push_back(prior);
      }
    }
    if (!survivors || !side) fail("FieldArmyExitReturningMemberRequired", p.army_id);
    std::sort(out.named_participants.begin(), out.named_participants.end());
    std::sort(out.held_named_participants.begin(), out.held_named_participants.end());
    out.actual_exit_at = *p.ready_at; out.adopted_at = *p.ready_applied_at;
    out.actual_anchor = take(project_field_army_exit_actual_point(x, catalog, a,
        p.battle_id, *side, target, source, b));
    return out;
  });
}
} // namespace hanmo::resident
