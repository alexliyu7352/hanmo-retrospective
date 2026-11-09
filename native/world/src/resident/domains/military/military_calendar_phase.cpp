#include "military_calendar_phase.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 校验完整半日期边界；无效期沿用原错误拒绝。
WorldMoment military_calendar_period_boundary(const ExactPeriodContext &p,
                                              ExactArithmeticBudget b) {
  if (p.period_id.empty() || p.through_tick <= 0 ||
      p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      p.through_tick != p.opening_at.half_day_tick + 1 ||
      compare(p.opening_at.sub_half_day, {}, b) != 0)
    fail("MilitaryCalendarPeriodRequired", p.period_id);
  return take(normalize_world_moment(p.through_tick, {}, b));
}
// 核对候选与原期一致，边界或起刻不匹配时拒绝。
void same_period(const MilitaryCalendarCandidate &c,
                 const ExactPeriodContext &p, ExactArithmeticBudget b) {
  military_calendar_period_boundary(p, b);
  if (c.period.period_id != p.period_id ||
      c.period.through_tick != p.through_tick ||
      !same_moment(c.period.opening_at, p.opening_at, b))
    fail("MilitaryCalendarCandidatePeriodMismatch", p.period_id);
}
// 核对共同世界仍处于原期开端，避免跨期写入。
void actual_common_period(const MilitaryCalendarWriteBinding &w,
                          const ExactPeriodContext &p,
                          ExactArithmeticBudget b) {
  military_calendar_period_boundary(p, b);
  if (w.common.tick() != p.opening_at.half_day_tick)
    fail("MilitaryCalendarCommonPeriodMismatch", p.period_id);
}
// 缺 battle owner 只代表明确未选旧围城；全目录、正文、头与本次请求均须阴性。
void validate_battle_dependency(const MilitaryCalendarWriteBinding &w,
                                const MilitaryCalendarCandidate &c) {
  if (!w.consumers && !w.field_without_siege)
    fail("MilitaryCalendarOriginalConsumerRequired");
  if (w.battles && !w.field_without_siege)
    return;
  const auto &m = w.military;
  const auto &catalog = w.catalog;
  if (!w.field_without_siege || !catalog.siege_topologies.empty() ||
      catalog.siege_engineering_policy || catalog.siege_integration.enabled ||
      !catalog.siege_integration.officer_policies.empty() ||
      !m.sieges.empty() || !m.siege_engineering.preparations.empty() ||
      !m.siege_lifecycle.facts.empty() || !m.siege_material_facts.empty() ||
      !m.siege_integration.proposals.empty() ||
      !m.siege_integration.actor_actions.empty() ||
      !m.siege_integration.decisions.empty() ||
      !m.siege_integration.attempts.empty() ||
      !c.actual_completed_battle_sources.empty() ||
      !c.original_siege_step.battle_requests.empty() ||
      !c.original_siege_step.events.empty() ||
      !c.actual_siege_entry_sources.empty())
    fail("MilitaryCalendarOriginalBattleOwnerRequired");
  for (const auto &stronghold : m.strongholds)
    if (!stronghold.siege_id.empty())
      fail("MilitaryCalendarOriginalBattleOwnerRequired", stronghold.id);
  for (const auto &contact : m.contact_facts)
    if (contact.siege_id)
      fail("MilitaryCalendarOriginalBattleOwnerRequired", contact.id);
  for (const auto &head : w.common.state().facts.rows())
    if (head.second.domain == "military" &&
        head.second.kind.find("siege") != StableId::npos)
      fail("MilitaryCalendarOriginalBattleOwnerRequired", head.first);
}
void append_unique(std::vector<StableId> &out, const std::vector<StableId> &ids,
                   const char *error) {
  for (const auto &id : ids) {
    if (id.empty() || std::find(out.begin(), out.end(), id) != out.end())
      fail(error, id);
    out.push_back(id);
  }
}
void append_journeys(std::vector<MilitaryJourneyRequest> &out,
                     const std::vector<MilitaryJourneyRequest> &requests) {
  for (const auto &request : requests) {
    if (request.army_id.empty() || request.destination_settlement_id.empty())
      fail("MilitaryActualJourneyRequestRequired", request.army_id);
    for (const auto &previous : out)
      if (previous.army_id == request.army_id)
        fail("MilitaryConflictingActualJourneyRequest", request.army_id);
    out.push_back(request);
  }
}
} // namespace
// 只为同一合法期建立候选，已用候选不重复初始化。
Status begin_military_calendar_candidate(MilitaryCalendarCandidate &c,
                                         const ExactPeriodContext &p,
                                         ExactArithmeticBudget b) {
  return status([&] {
    military_calendar_period_boundary(p, b);
    if (!c.period.period_id.empty()) {
      same_period(c, p, b);
      return;
    }
    if (c.siege_period_consumed || c.siege_battles_consumed ||
        c.siege_results_consumed || c.campaign_consumed ||
        !c.original_siege_step.events.empty() ||
        !c.original_siege_step.battle_requests.empty() ||
        !c.actual_completed_battle_sources.empty() ||
        !c.completed_receipt_ids.empty() || !c.actual_journey_requests.empty())
      fail("MilitaryCalendarCandidateAlreadyUsed");


    (c.copied_fields).grow(0);
    c.period = ExactPeriodContext{checkpoint_fresh_string_copy(p.period_id),
                                  p.opening_at, p.through_tick};
  });
}
// 仅登记原期边界的真实战斗终局，同源重放不重复登记。
Status register_actual_military_terminal_for_calendar(
    MilitaryCalendarCandidate &c, const MilitaryCalendarBattleWrite &battles,
    const MomentOwnerSource &source, const WorldMoment &at,
    ExactArithmeticBudget b) {
  return status([&] {
    if (c.period.period_id.empty() || !c.siege_period_consumed ||
        c.siege_battles_consumed ||
        !same_moment(at, military_calendar_period_boundary(c.period, b), b))
      fail("MilitaryCalendarTerminalWrongStage", source.source_ref.id);
    take(battles.validate_actual_terminal(source, at));
    for (const auto &old : c.actual_completed_battle_sources)
      if (same_source(old.source_ref, source.source_ref)) {
        if (old.source_schema != source.source_schema)
          fail("MilitaryTerminalSchemaMismatch", source.source_ref.id);
        return;
      }


    (c.copied_fields).grow(0);
    c.actual_completed_battle_sources.push_back(source);
  });
}
Status apply_military_siege_period_in_candidate(MilitaryCalendarWriteBinding &w,
                                                MilitaryCalendarCandidate &c,
                                                const ExactPeriodContext &p,
                                                ExactPhaseWriteContext &,
                                                ExactArithmeticBudget b) {
  return status([&] {
    take(begin_military_calendar_candidate(c, p, b));
    same_period(c, p, b);
    actual_common_period(w, p, b);
    validate_battle_dependency(w, c);
    if (c.siege_period_consumed || c.siege_battles_consumed ||
        c.siege_results_consumed || c.campaign_consumed)
      fail("MilitarySiegePeriodAlreadyConsumed", p.period_id);
    take(preflight_siege_world(w.common, w.military, w.catalog, w.access));
    auto actual = take(step_siege_world(w.common, w.military, w.catalog,
                                        w.access, p.through_tick));
    c.original_siege_step = std::move(actual);
    validate_battle_dependency(w, c);
    c.siege_period_consumed = true;
  });
}
// 按原期边界消费已核实的战斗终局，非法阶段拒绝。
Status apply_military_siege_battles_finished_in_candidate(
    MilitaryCalendarWriteBinding &w, MilitaryCalendarCandidate &c,
    const ExactPeriodContext &p, ExactPhaseWriteContext &context,
    ExactArithmeticBudget b) {
  return status([&] {
    same_period(c, p, b);
    actual_common_period(w, p, b);
    validate_battle_dependency(w, c);
    if (!c.siege_period_consumed || c.siege_battles_consumed ||
        c.siege_results_consumed || c.campaign_consumed)
      fail("MilitarySiegeBattleFinishStageMismatch", p.period_id);
    const auto at = military_calendar_period_boundary(p, b);
    for (const auto &source : c.actual_completed_battle_sources) {
      take(w.battles->validate_actual_terminal(source, at));
      take(w.battles->finish_actual_siege_battle_in_candidate(source, at,
                                                              context));
    }
    c.siege_battles_consumed = true;
  });
}
// 按原序消费围城结果及后果，保持真实来源和阶段约束。
Status apply_military_siege_results_in_candidate(
    MilitaryCalendarWriteBinding &w, MilitaryCalendarCandidate &c,
    const ExactPeriodContext &p, ExactPhaseWriteContext &context,
    ExactArithmeticBudget b) {
  return status([&] {
    same_period(c, p, b);
    actual_common_period(w, p, b);
    validate_battle_dependency(w, c);
    if (!c.siege_period_consumed || !c.siege_battles_consumed ||
        c.siege_results_consumed || c.campaign_consumed)
      fail("MilitarySiegeResultsStageMismatch", p.period_id);
    const auto at = military_calendar_period_boundary(p, b);
    for (const auto &request : c.original_siege_step.battle_requests) {
      auto source = take(w.battles->create_actual_siege_battle_in_candidate(
          request.siege_id, request.request, at, context));
      take(w.battles->validate_actual_siege_entry(request.siege_id,
                                                  request.request, source, at));
      c.actual_siege_entry_sources.push_back(std::move(source));
    }
    // 保原首个siege出现顺序；每组只借原结果，不复制全部事件/字符串或建立第二索引。
    for (std::size_t index = 0; index < c.original_siege_step.events.size();
         ++index) {
      const auto &entry = c.original_siege_step.events[index];
      bool earlier = false;
      for (std::size_t previous = 0; previous < index; ++previous)
        if (c.original_siege_step.events[previous].siege_id == entry.siege_id) {
          earlier = true;
          break;
        }
      if (earlier)
        continue;
      SiegeWorldEventRange events(c.original_siege_step, entry.siege_id);
      auto actual = take(finish_siege_events(
          w.common, w.military, w.catalog, w.access, entry.siege_id, events,
          p.through_tick,
          MilitaryOccupationExecution{
              MilitaryOccupationProducerKind::calendar_siege_results, {}}));
      append_unique(c.completed_receipt_ids, actual.completed_receipt_ids,
                    "DuplicateMilitaryActualReceipt");
      append_journeys(c.actual_journey_requests, actual.journey_requests);
      // A real event is not an empty calendar, even if its actor-change list
      // happens to be empty. Never discard its remaining consequences.
      if (!w.consumers)
        fail("MilitaryCalendarOriginalConsumerRequired", entry.siege_id);
      auto effects =
          take(w.consumers->apply_actual_military_consequences_in_candidate(
              actual, at, context));
      for (auto &source : effects) {
        if (!source_valid(source.source_ref) || source.source_schema < 1)
          fail("MilitaryActualConsequenceSourceRequired");
        c.actual_consequence_sources.push_back(std::move(source));
      }
    }
    c.siege_results_consumed = true;
  });
}
// 在原期边界接续真实军旅行程，重复或越序阶段拒绝。
Status apply_military_campaign_adoption_in_candidate(
    MilitaryCalendarWriteBinding &w, MilitaryCalendarCandidate &c,
    const ExactPeriodContext &p, ExactPhaseWriteContext &context,
    ExactArithmeticBudget b) {
  return status([&] {
    same_period(c, p, b);
    actual_common_period(w, p, b);
    validate_battle_dependency(w, c);
    if (!c.siege_results_consumed || c.campaign_consumed)
      fail("MilitaryCampaignStageMismatch", p.period_id);
    auto actual =
        take(step_military_campaigns(w.common, w.military, w.catalog, w.access,
                                     p.through_tick, c.completed_receipt_ids));
    take(release_invalid_military_commands(w.common, w.military, w.catalog,
                                           w.access, p.through_tick));
    append_journeys(actual, c.actual_journey_requests);
    const auto at = military_calendar_period_boundary(p, b);
    for (const auto &request : actual) {
      // step_military_campaigns has already produced the original request.
      // Missing ownership is an error of the whole candidate, never a Depart
      // substitution or a partially published return.
      if (!w.consumers)
        fail("MilitaryCalendarOriginalConsumerRequired", request.army_id);
      take(w.consumers->begin_actual_military_journey_in_candidate(request, at,
                                                                   context));
    }
    c.actual_journey_requests = std::move(actual);
    c.campaign_consumed = true;
  });
}
Status validate_military_calendar_candidate_consumed(
    const MilitaryCalendarCandidate &c, const ExactPeriodContext &p,
    ExactArithmeticBudget b) {
  return status([&] {
    same_period(c, p, b);
    if (!c.siege_period_consumed || !c.siege_battles_consumed ||
        !c.siege_results_consumed || !c.campaign_consumed)
      fail("MilitaryCalendarCandidateNotFullyConsumed", p.period_id);
  });
}
} // namespace hanmo::resident
