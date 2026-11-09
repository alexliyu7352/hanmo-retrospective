#include "field_army_exit_rally.hpp"
#include "field_ability_sources.hpp"
#include "field_author_data.hpp"
#include "field_winner_continuation.hpp"
#include "field_boundary_continuation_source.hpp"
#include "field_binding_source.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_navigation.hpp"
#include "field_roster_origin.hpp"
#include "resident/domains/military/training_exact.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 局部时间比较不改变世界时钟。
bool time_equal(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
bool point_equal(PointI a, PointI b) { return a.x == b.x && a.y == b.y; }
bool before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
// 原初态逐项比较；终局后的持久位置只在steps，绝不回写session。
void require_origin(const FieldArmyExitPlan &p, const FieldArmyExitRallyOriginRead &r) {
  if (!p.ordinary_deadline || !time_equal(*p.ordinary_deadline, r.ordinary_deadline) ||
      p.rally_formations.size() != r.formations.size())
    fail("FieldArmyExitRallyOriginMismatch", p.fact_id);
  for (std::size_t i = 0; i < r.formations.size(); ++i) {
    const auto &a = p.rally_formations[i], &b = r.formations[i];
    if (a.formation_id != b.formation_id || !point_equal(a.initial_position_dm, b.initial_position_dm) ||
        a.ordinary_member_ids != b.ordinary_member_ids || a.retained_officer_ids != b.retained_officer_ids ||
        bool(a.original_exit) != bool(b.original_exit) ||
        (a.original_exit && !same_source(*a.original_exit, *b.original_exit)))
      fail("FieldArmyExitRallyOriginMismatch", p.fact_id);
  }
}
// 仅归约指定真实Common前缀的持久位置；完整geometry由外层逐步验证，不递归另一军。
FieldArmyExitProgressRead prefix(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const FieldArmyExitPlan &p, std::size_t stop) {
  if (!p.terminal_segment) fail("FieldArmyExitRallyOriginRequired", p.fact_id);
  const auto initial = take(read_field_army_exit_rally_origin(x, catalog, a, p, *p.terminal_segment));
  require_origin(p, initial);
  FieldArmyExitProgressRead out{&p, initial.terminal->settled_at, {}};
  for (const auto &row : p.rally_formations)
    out.formations.push_back({&row, row.initial_position_dm, row.ordinary_member_ids});
  auto previous = take(read_actual_common_fact_position(x.common, *p.terminal_segment));
  for (const auto &step : p.steps) {
    const auto at = take(read_actual_common_fact_position(
        x.common, SourceRef{"military", "field_army_exit_step", step.fact_id}));
    if (at <= previous) fail("FieldArmyExitStepSourceOrderRequired", step.fact_id);
    previous = at;
    if (at >= stop) break;
    if (!time_equal(step.from, out.at) || step.formations.size() != out.formations.size())
      fail("FieldArmyExitStepPrefixMismatch", step.fact_id);
    const auto next = take(add_field_battle_seconds(step.from, catalog.parameters.step_s, catalog.parameters));
    if (!time_equal(next, step.to)) fail("FieldMotionActualStepRequired", step.fact_id);
    for (std::size_t i = 0; i < out.formations.size(); ++i) {
      auto &row = out.formations[i]; const auto &m = step.formations[i];
      if (m.formation_id != row.origin->formation_id || !point_equal(m.from_dm, row.position_dm))
        fail("FieldArmyExitStepPrefixMismatch", step.fact_id);
      if (!m.deserted_member_ids.empty()) {
        if (!p.ordinary_deadline || before(step.to, *p.ordinary_deadline) ||
            m.reached || m.deserted_member_ids != row.ordinary_member_ids || row.origin->original_exit)
          fail("FieldArmyExitOrdinaryDeadlineSourceRequired", step.fact_id);
        row.ordinary_member_ids.clear();
      }
      row.position_dm = m.to_dm;
    }
    out.at = step.to;
  }
  return out;
}
// 整军口由原生成几何重建；不可把单个formation旧出口当全军聚集点。
GeneratedBattleField geometry(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldArmyExitPlan &p) {
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : x.field.contacts) if (row.battle_id == p.battle_id) {
    if (contact) fail("FieldArmyExitOriginalContactRequired", p.battle_id);
    contact = &row;
  }
  if (!contact) fail("FieldArmyExitOriginalContactRequired", p.battle_id);
  auto field = take(regenerate_field_contact_battle(x.travel, catalog, contact->source, &x));
  if (field.field_hash != p.field_hash || p.exit_cell >= field.cells.size())
    fail("FieldArmyExitOriginalGeometryRequired", p.fact_id);
  return field;
}
// 兵员仍用原编队外接圆；普通兵全失后才按真实保留独立将领人数行走，不伪造兵籍。
Integer moving_count(const FieldFormation &f, const FieldArmyExitFormationProgress &p) {
  std::size_t count = p.ordinary_member_ids.size();
  for (const auto &actor : p.origin->retained_officer_ids)
    if (std::find(f.fit.begin(), f.fit.end(), actor) != f.fit.end() ||
        std::find(f.wounded.begin(), f.wounded.end(), actor) != f.wounded.end()) ++count;
  if (count == 0) count = p.origin->retained_officer_ids.size();
  if (count > 500) fail("FieldArmyExitFormationMemberBoundRequired", f.formation_id);
  return Integer(count);
}
// 退出口到齐者已离开局部碰撞圆但仍由Army原hold保留；不得先释放其钱包/库存。
bool reached(const FieldArmyExitFormationProgress &p, PointI target) {
  return p.origin->original_exit.has_value() || point_equal(p.position_dm, target);
}
bool has_members(const FieldArmyExitFormationProgress &p) {
  return !p.ordinary_member_ids.empty() || !p.origin->retained_officer_ids.empty();
}
bool all_arrived(const FieldArmyExitProgressRead &p, PointI target) {
  return std::all_of(p.formations.begin(), p.formations.end(), [&](const auto &row) {
    return !has_members(row) || reached(row, target);
  });
}
// 胜方真的已接原世界余程后才离开局部碰撞集合。今日binding缺失不是历史证据；
// 先借原engagement完整reader，再比较采用头与本step的真实Common前缀。
bool winner_left_before(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const StableId &battle, const StableId &army, std::size_t cut) {
  const auto *binding = take(read_field_original_battle_binding(x.common, x.field, battle, army));
  std::optional<SourceRef> found;
  const auto accept = [&](const SourceRef &source) {
    if (take(read_actual_common_fact_position(x.common, source)) >= cut) return;
    if (found) fail("FieldArmyExitWinnerContinuationAmbiguous", army);
    if (const auto *boundary = std::get_if<FieldBoundaryContactBinding>(&binding->source)) {
      if (!a.diplomacy) fail("FieldBoundaryContinuationSameWorldRequired", army);
      take(read_field_boundary_continuation_source(x, *a.diplomacy, catalog,
          {"military", "field_battle_contact", boundary->contact_id}, {}, &source));
    } else take(read_field_winner_continuation(x, catalog, battle, army, &source));
    found = source;
  };
  if (const auto *boundary = std::get_if<FieldBoundaryContactBinding>(&binding->source)) {
    for (const auto &[id, row] : x.movement.boundary_continuations.rows())
      if (same_source(row.field_contact,
              {"military", "field_battle_contact", boundary->contact_id}))
        accept({"travel", "movement_boundary_continuation", id});
  } else {
    for (const auto &[id, row] : x.movement.contact_continuations.rows()) {
      const auto *unit = x.movement.units.find(row.closed_unit_ref);
      const auto *group = unit ? std::get_if<GroupMovementExecution>(&unit->execution) : nullptr;
      const auto *carrier = group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      if (!carrier || carrier->army_id != army || row.actual_resolution_source.domain != "military" ||
          row.actual_resolution_source.kind != "field_battle_segment") continue;
      const auto terminal = take(read_field_segment_settlement_source(x.common, x.military,
          x.field, row.actual_resolution_source, &x.clock));
      if (terminal.session->battle_id == battle) accept({"travel", "movement_contact_continuation", id});
    }
    for (const auto &[id, row] : x.movement.arrivals.rows()) {
      const auto *anchor = std::get_if<OriginalMovementAnchor>(&row.actual_anchor);
      const auto *group = std::get_if<GroupMovementExecution>(&row.execution);
      const auto *carrier = group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      if (!anchor || !carrier || carrier->army_id != army || anchor->actual_source.domain != "military" ||
          anchor->actual_source.kind != "field_battle_segment") continue;
      const auto terminal = take(read_field_segment_settlement_source(x.common, x.military,
          x.field, anchor->actual_source, &x.clock));
      if (terminal.session->battle_id == battle) accept({"travel", "movement_arrival", id});
    }
  }
  return found.has_value();
}
// 只读原step几何：原A*、兵种步速、Guard/Charge、地形与碰撞都继续实际计算。
FieldArmyExitStep expected_step(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const FieldArmyExitProgressRead &current, const GeneratedBattleField &field,
    const WorldMoment &applied_at, std::size_t common_cut) {
  const auto &p = *current.plan;
  const auto &s = x.field.sessions.at(p.battle_id);
  FieldArmyExitStep out;
  out.from = current.at;
  out.to = take(add_field_battle_seconds(out.from, field.parameters.step_s, field.parameters));
  out.applied_at = applied_at;
  std::map<StableId, FieldMotionPrefixFormation> positions;
  for (const auto &[id, f] : s.formations)
    positions.emplace(id, FieldMotionPrefixFormation{f.position_dm, Integer(f.fit.size()), false,
        f.state == FieldFormationState::Withdrawn || f.state == FieldFormationState::Destroyed, true});
  std::set<StableId> rally_armies, other_armies;
  for (const auto &[id, plan] : x.field.army_exit_plans)
    if (plan.battle_id == p.battle_id && plan.terminal_segment) rally_armies.insert(plan.army_id);
  for (const auto &[id, f] : s.formations) if (!rally_armies.contains(f.army_id)) other_armies.insert(f.army_id);
  for (const auto &army : other_armies)
    if (winner_left_before(x, catalog, a, p.battle_id, army, common_cut))
      for (const auto &[id, f] : s.formations) if (f.army_id == army) positions.at(id).withdrawn = true;
  for (const auto &[id, other] : x.field.army_exit_plans) {
    if (other.battle_id != p.battle_id || !other.terminal_segment) continue;
    const auto state = prefix(x, catalog, a, other, common_cut);
    const auto target = field_cell_center_dm(field, other.exit_cell);
    for (const auto &row : state.formations) {
      const auto f = s.formations.find(row.origin->formation_id);
      if (f == s.formations.end()) fail("FieldArmyExitActualFormationRequired", row.origin->formation_id);
      positions.at(f->first) = {row.position_dm, moving_count(f->second, row), false,
                               !has_members(row) || reached(row, target), true};
    }
  }
  const auto target = field_cell_center_dm(field, p.exit_cell);
  const FieldBattleWindowSlice step{out.from, out.to, s.segment, 1, false, out.to.t == 0};
  for (const auto &row : current.formations) {
    const auto &f = s.formations.at(row.origin->formation_id);
    FieldArmyExitMotion motion;
    motion.formation_id = f.formation_id;
    motion.from_dm = motion.to_dm = row.position_dm;
    motion.reached = reached(row, target);
    if (!motion.reached && has_members(row)) {
      bool confused = false;
      for (const auto &status : f.statuses)
        if (status.kind == FieldStatusKind::Confused && status.until && before(out.from, *status.until))
          confused = true;
      if (confused) motion.blocked = true;
      else {
        // 终局已解除Engaged/待首令的战斗任务；新已批汇合命令不是重开combat pulse。
        const FieldMotionPrefixRead projected{out.from, positions};
        const auto move = take(read_field_navigation_step_at_prefix(
            s, field, f.formation_id, target, step, &projected));
        motion.to_dm = move.to_dm; motion.reached = move.reached; motion.blocked = move.blocked;
      }
      if (!motion.reached && !before(out.to, *p.ordinary_deadline))
        motion.deserted_member_ids = row.ordinary_member_ids;
    }
    auto &position = positions.at(f.formation_id);
    position.position_dm = motion.to_dm;
    position.withdrawn = motion.reached ||
        (!motion.deserted_member_ids.empty() && row.origin->retained_officer_ids.empty());
    out.formations.push_back(std::move(motion));
  }
  return out;
}
// 对称比较每个成员退出和每步几何；不能只检查最后一个缓存位置。
void equal_step(const FieldArmyExitStep &a, const FieldArmyExitStep &b) {
  if (!time_equal(a.from, b.from) || !time_equal(a.to, b.to) || a.formations.size() != b.formations.size())
    fail("FieldArmyExitStepSourceMismatch", a.fact_id);
  for (std::size_t i = 0; i < a.formations.size(); ++i) {
    const auto &x = a.formations[i], &y = b.formations[i];
    if (x.formation_id != y.formation_id || !point_equal(x.from_dm, y.from_dm) ||
        !point_equal(x.to_dm, y.to_dm) || x.reached != y.reached || x.blocked != y.blocked ||
        x.deserted_member_ids != y.deserted_member_ids)
      fail("FieldArmyExitStepSourceMismatch", a.fact_id);
  }
}
// 实际采用仍是原窗口closing；原period opening和Common顺序阻止把今日写入回填旧窗。
void validate_step_application(const FieldExactContactReadContext &x,
    const FieldArmyExitStep &step, std::size_t position) {
  const auto now = take(x.clock.read_current_moment());
  if (!x.periods || step.from.tick == std::numeric_limits<Integer>::max() ||
      step.applied_at.half_day_tick != step.from.tick + 1 ||
      take(compare_fractions(step.applied_at.sub_half_day, CanonicalFraction{})) != 0 ||
      take(compare_world_moments(step.applied_at, now)) > 0)
    fail("FieldArmyExitActualClosingRequired", step.fact_id);
  const auto opened = take(x.periods->read_training_period(step.from.tick));
  if (opened.from_tick != step.from.tick || opened.to_tick != step.from.tick + 1 ||
      opened.original_opening.source_schema != 1 ||
      opened.opening_at.half_day_tick != step.from.tick ||
      take(compare_fractions(opened.opening_at.sub_half_day, CanonicalFraction{})) != 0 ||
      take(read_actual_common_fact_position(x.common, opened.original_opening.source_ref)) >= position)
    fail("FieldArmyExitOriginalWindowRequired", step.fact_id);
  if (same_moment(step.applied_at, now)) {
    if (!same_moment(step.applied_at, take(x.clock.read_field_window_closing_moment())))
      fail("FieldArmyExitActualClosingRequired", step.fact_id);
  } else {
    const auto after = take(x.periods->read_training_period(step.applied_at.half_day_tick));
    if (!same_moment(after.opening_at, step.applied_at) ||
        after.original_opening.source_schema != 1 ||
        take(read_actual_common_fact_position(x.common, after.original_opening.source_ref)) <= position)
      fail("FieldArmyExitOriginalWindowRequired", step.fact_id);
  }
}
}
// 独立原历史读口：不调用Complete、不推进RNG/时钟，也不把新step当成已付S1费用。
Outcome<FieldArmyExitProgressRead> read_field_army_exit_progress(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &choice) {
  return guard<FieldArmyExitProgressRead>([&] {
    if (!x.original_military_access) fail("FieldArmyExitOriginalMilitaryOwnerRequired", choice.id);
    if (a.exact_contacts != &x || !a.original_rosters ||
        &a.original_rosters->common != &x.common || &a.original_rosters->military != &x.military ||
        x.periods != &a.original_rosters->periods)
      fail("FieldArmyExitSameWorldSourcesRequired", choice.id);
    const auto *p = take(read_field_army_exit_choice_source(x, catalog, a,
        *x.original_military_access, choice));
    // 全场按真实Common头合并验证。后一步使用别军前缀前，那个前缀的几何已先证明；
    // 不做A军→B军→A军递归，也不以本军自洽接受别军伪位置。
    struct OrderedStep { std::size_t position; const FieldArmyExitPlan *plan; const FieldArmyExitStep *step; };
    std::vector<OrderedStep> ordered;
    for (const auto &[id, other] : x.field.army_exit_plans) {
      if (other.battle_id != p->battle_id || !other.terminal_segment) continue;
      take(read_field_army_exit_choice_source(x, catalog, a, *x.original_military_access,
          {"military", "field_army_exit_choice", id}));
      // 即使尚无一步也核真实origin，不能让伪初态作为另一军碰撞物。
      prefix(x, catalog, a, other, 0);
      for (const auto &step : other.steps)
        ordered.push_back({take(read_actual_common_fact_position(x.common,
            SourceRef{"military", "field_army_exit_step", step.fact_id})), &other, &step});
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto &l, const auto &r) { return l.position < r.position; });
    std::optional<std::size_t> last;
    for (const auto &actual : ordered) {
      if (last && *last == actual.position) fail("FieldArmyExitStepSourceAmbiguous", actual.step->fact_id);
      last = actual.position;
      const auto field = geometry(x, catalog, *actual.plan);
      validate_step_application(x, *actual.step, actual.position);
      const auto state = prefix(x, catalog, a, *actual.plan, actual.position);
      if (all_arrived(state, field_cell_center_dm(field, actual.plan->exit_cell)))
        fail("FieldArmyExitMotionAfterArrival", actual.step->fact_id);
      equal_step(*actual.step, expected_step(x, catalog, a, state, field,
          actual.step->applied_at, actual.position));
    }
    return prefix(x, catalog, a, *p, std::numeric_limits<std::size_t>::max());
  });
}
} // namespace hanmo::resident

#include "field_binding_source.hpp"
namespace hanmo::resident {
// 实际候选写者逐军逐步推进；不调用世界Advance，不改原combat result/formation位置。
Outcome<bool> advance_field_army_exit_rally_step_in_candidate(CommonStore &common,
    FieldBattleWorldState &world, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const StableId &battle,
    CheckpointMemoryCharge &owner) {
  return guard<bool>([&] {
    if (!a.exact_contacts || &a.exact_contacts->common != &common ||
        &a.exact_contacts->field != &world) fail("FieldArmyExitSameWorldSourcesRequired", battle);
    const auto &x = *a.exact_contacts;
    const auto now = take(x.clock.read_current_moment());
    if (!same_moment(now, take(x.clock.read_field_window_closing_moment())))
      fail("FieldArmyExitActualClosingRequired", battle);
    FieldArmyExitPlan *selected = nullptr;
    std::optional<FieldArmyExitProgressRead> selected_state;
    for (auto &[id, p] : world.army_exit_plans) {
      if (p.battle_id != battle || !p.terminal_segment || p.ready_source) continue;
      auto state = take(read_field_army_exit_progress(x, catalog, a,
          {"military", "field_army_exit_choice", id}));
      const auto field = geometry(x, catalog, p);
      if (all_arrived(state, field_cell_center_dm(field, p.exit_cell))) {
        std::size_t survivors = 0;
        for (const auto &f : state.formations)
          survivors += f.ordinary_member_ids.size() + f.origin->retained_officer_ids.size();
        // 全普通兵均已有真实逃散源且无活将领：不制造空ready/空军旅；原calendar
        // 仍自行给CommanderUnavailable等等待结果，军产不凭本几何层销账。
        if (!survivors) continue;
        const auto closing_tick = __int128_t(state.at.tick) + (state.at.t ? 1 : 0);
        if (closing_tick != now.half_day_tick)
          fail("FieldArmyExitReadyActualWindowRequired", p.army_id);
        const auto fact = take(common.next_owned_id("fact"));
        const SourceRef source{"military", "field_army_exit_ready", fact.id()};
        owner.grow(0); // 保持原 job 检查与真实归属，不再逐字段记账。
        take(common.append_fact_borrowed(fact.id(), "field_army_exit_ready", "military"));
        p.ready_source = source; p.ready_at = state.at; p.ready_applied_at = now;
        return true;
      }
      if (state.at.tick == now.half_day_tick && state.at.t == 0) continue;
      if (__int128_t(state.at.tick) + 1 != now.half_day_tick)
        fail("FieldArmyExitUnconsumedWindowRequired", p.army_id);
      if (!selected || before(state.at, selected_state->at) ||
          (time_equal(state.at, selected_state->at) && p.army_id < selected->army_id)) {
        selected = &p; selected_state = std::move(state);
      }
    }
    if (!selected) return false;
    const auto field = geometry(x, catalog, *selected);
    const auto common_cut = take(read_current_common_fact_prefix(common));
    if (common_cut.common_fact_count < 0) fail("CommonActualFactPrefixRequired", battle);
    auto step = expected_step(x, catalog, a, *selected_state, field, now,
                              std::size_t(common_cut.common_fact_count));
    if (step.to.tick > now.half_day_tick ||
        (step.to.tick == now.half_day_tick && step.to.t != 0))
      fail("FieldArmyExitActualClosingRequired", selected->army_id);
    const auto fact = take(common.next_owned_id("fact"));
    step.fact_id = fact.id();
    owner.grow(0); // 持久 step 仍由原资源拥有，真实 vector 自行分配。
    take(common.append_fact_borrowed(step.fact_id, "field_army_exit_step", "military"));
    selected->steps.push_back(std::move(step));
    return true;
  });
}
// 从实际Common头定位唯一step，再沿完整原几何历史验证，不让任意同名fact授后果。
Outcome<FieldArmyExitStepSourceRead> read_field_army_exit_step_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &source) {
  return guard<FieldArmyExitStepSourceRead>([&] {
    if (source.domain != "military" || source.kind != "field_army_exit_step")
      fail("FieldArmyExitStepSourceRequired", source.id);
    take(read_actual_common_fact_position(x.common, source));
    FieldArmyExitStepSourceRead out;
    for (const auto &[id, plan] : x.field.army_exit_plans)
      for (const auto &step : plan.steps) if (step.fact_id == source.id) {
        if (out.step) fail("FieldArmyExitStepSourceAmbiguous", source.id);
        out.plan = &plan; out.step = &step;
      }
    if (!out.step) fail("FieldArmyExitStepSourceRequired", source.id);
    take(read_field_army_exit_progress(x, catalog, a,
        {"military", "field_army_exit_choice", out.plan->fact_id}));
    const auto session = x.field.sessions.find(out.plan->battle_id);
    if (session == x.field.sessions.end()) fail("FieldBattleNotFound", out.plan->battle_id);
    out.session = &session->second;
    return out;
  });
}
// deadline普通兵真实后果读口供唯一S4/原成员loss消费者使用，不写第二伤亡账。
Outcome<FieldArmyExitOrdinaryDepartureRead> read_field_army_exit_ordinary_departure_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &source, const StableId &actor) {
  return guard<FieldArmyExitOrdinaryDepartureRead>([&] {
    FieldArmyExitOrdinaryDepartureRead out;
    out.source = take(read_field_army_exit_step_source(x, catalog, a, source));
    const auto *binding = take(read_field_original_battle_binding(x.common, x.field,
        out.source.plan->battle_id, out.source.plan->army_id));
    for (const auto &motion : out.source.step->formations)
      if (std::find(motion.deserted_member_ids.begin(), motion.deserted_member_ids.end(), actor) !=
          motion.deserted_member_ids.end()) {
        if (out.motion) fail("FieldArmyExitOrdinaryActorAmbiguous", actor);
        const auto f = out.source.session->formations.find(motion.formation_id);
        if (f == out.source.session->formations.end() || f->second.army_id != out.source.plan->army_id ||
            f->second.commander_actor_id == actor || f->second.deputy_actor_id == actor)
          fail("FieldArmyExitOrdinaryActorRequired", actor);
        out.motion = &motion; out.formation = &f->second;
      }
    if (!out.motion) fail("FieldArmyExitOrdinaryActorRequired", actor);
    for (const auto &member : binding->members)
      if (member.actor_id == actor && member.formation_id == out.formation->formation_id) {
        if (out.member) fail("FieldArmyExitOrdinaryActorAmbiguous", actor);
        out.member = &member;
      }
    // 全兵员后果保留原ID；无Common Actor的普通兵不制造S4/S1人物引用。
    if ((x.common.actor(actor) && !out.member) || (!x.common.actor(actor) && out.member))
      fail("FieldArmyExitOriginalMemberRequired", actor);
    return out;
  });
}
} // namespace hanmo::resident
