#include "social_exact_phase.hpp"
#include "resident/domains/director/director_knowledge_wake2.hpp"
#include "resident/domains/director/director_task_wake2.hpp"
#include "resident/kernel/economic_supply_policy_integration.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "social_period_sources.hpp"
#include "social_relation_execution.hpp"
#include "social_time2.hpp"
#include "social_time2_version.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> bad(const char *c, const StableId &id = {}) {
  return {false, {}, common_failure(c, id, ErrorKind::internal).error};
}
Status selected(const WorldBundle &w) {
  return w.catalog->social && w.catalog->common && w.world_timeline &&
                 w.social_period_sources && w.social.dynamic &&
                 w.social.dynamic->time2 &&
                 social_time2_version(w.social.dynamic->version)
             ? common_success()
             : common_failure("ExactSocialSourcePairRequired");
}
Outcome<ExactPeriodOwnerSources> period_sources(const WorldBundle &w,
                                                const ExactPeriodContext &p) {
  auto valid = selected(w);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *o = w.social_period_sources->openings.find(p.period_id);
  if (!o || o->through_tick != p.through_tick)
    return bad<ExactPeriodOwnerSources>("SocialOpeningSourceMissing",
                                        p.period_id);
  auto at = compare_world_moments(o->opened_at, p.opening_at);
  if (!at.ok)
    return {false, {}, at.error};
  if (at.value)
    return bad<ExactPeriodOwnerSources>("SocialOpeningMomentMismatch",
                                        p.period_id);
  ExactPeriodOwnerSources out;
  out.opening_source = {"social", "social_period_open", p.period_id};
  out.opening_schema = 1;
  if (const auto *c = w.social_period_sources->closures.find(p.period_id)) {
    if (c->delivered_message_ids != o->due_message_ids)
      return bad<ExactPeriodOwnerSources>("SocialClosingSourceMismatch",
                                          p.period_id);
    out.closing_source =
        SourceRef{"social", "social_period_close", p.period_id};
    out.closing_schema = 1;
  }
  return {true, std::move(out), {}};
}
struct WriteBoundary {
  WorldBundle &world;
  explicit WriteBoundary(WorldBundle &w) : world(w) {
    world.note_write_boundary();
  }
  ~WriteBoundary() { world.note_write_boundary(); }
};
template <class Fn> auto write_leaf(WorldBundle &w, Fn &&fn) {
  WriteBoundary boundary(w);
  return fn();
}
Status collect_reaction(WorldBundle &w, const WorldMoment &now) {
  // This is the original final supply projection, after political_due_close and
  // before collection. It never adds a message to the frozen due delivery set.
  std::optional<DynamicStepInput> phase;
  if (now.half_day_tick != w.common.tick()) {
    if (w.common.tick() == std::numeric_limits<Integer>::max() ||
        now.half_day_tick != w.common.tick() + 1)
      return common_failure("SocialProjectionPhaseMismatch");
    auto integral = compare_world_moments(now, {now.half_day_tick, {}});
    if (!integral.ok)
      return {false, {}, integral.error};
    if (integral.value)
      return common_failure("SocialProjectionPhaseMismatch");
    phase.emplace();
    phase->from_tick = w.common.tick();
    phase->to_tick = now.half_day_tick;
  }
  auto valid = write_leaf(w, [&] {
    return project_economic_supply_world_in_candidate(w, phase ? &*phase
                                                               : nullptr);
  });
  if (!valid.ok)
    return valid;
  valid = write_leaf(w, [&]() -> Status {
    auto source = capture_exact_result_time_read(w);
    if (!source.ok)
      return {false, {}, source.error};
    return collect_exact_dynamic_reports(w.common, w.social, *source.value,
                                         source.value->stamp(), now,
                                         w.catalog->identity);
  });
  if (!valid.ok)
    return valid;
  if (w.director.subject2) {
    valid = write_leaf(w, [&]() -> Status {
      auto source = capture_exact_result_time_read(w);
      if (!source.ok)
        return {false, {}, source.error};
      return collect_director_knowledge_wakes2(
          w.common, w.social, *source.value, source.value->stamp(), w.director,
          now);
    });
    if (!valid.ok)
      return valid;
    valid = write_leaf(w, [&] {
      return collect_director_task_commitment_wakes2(
          w.common, w.tasks, w.affairs, w.director, now);
    });
    if (!valid.ok)
      return valid;
  }

  return common_success();
}

} // namespace
// 原三条正文只提成具名叶；完整组合借同一函数，不复制回调拥有对象。
Status validate_original_social_phase_selection(const WorldBundle &w,
                                                Integer initial_tick) {
  auto s = selected(w);
  if (!s.ok)
    return s;
  return initial_tick >= 0 && initial_tick <= w.common.tick()
             ? common_success()
             : common_failure("InvalidSocialInitialBoundary");
}
Status validate_original_social_phase_sources(const WorldBundle &w,
                                              const WorldMoment &now,
                                              Integer initial_tick) {
  auto s = selected(w);
  if (!s.ok)
    return s;
  auto source = capture_exact_result_time_read(w);
  if (!source.ok)
    return Status{false, {}, source.error};
  auto equal = compare_world_moments(source.value->source_moment(), now);
  if (!equal.ok)
    return Status{false, {}, equal.error};
  if (equal.value)
    return common_failure("SocialPhaseReadMomentMismatch");
  s = validate_social_time2(w.common, w.social, *source.value,
                            source.value->stamp(), w.catalog->identity);
  if (!s.ok)
    return s;
  s = validate_social_at_moment(w.common, w.social, *w.catalog->common,
                                &w.services, now);
  if (!s.ok)
    return s;
  return validate_social_due_periods(
      w.common, w.social, *w.social_period_sources, *w.world_timeline,
      w.catalog->identity, initial_tick);
}
Status apply_original_social_calendar_stage(
    WorldBundle &w, ExactStage stage, const ExactPeriodContext &p,
    ExactPhaseWriteContext &,
    SocialCalendarDispatchFunction selected_dispatch) {
  auto valid = selected(w);
  if (!valid.ok)
    return valid;
  auto now = w.actual_phase_moment();
  if (!now.ok)
    return Status{false, {}, now.error};
  if (stage == ExactStage::knowledge_reaction_close)
    return collect_reaction(w, now.value);
  if (stage != ExactStage::social_exchange)
    return common_failure("InvalidSocialCalendarStage");
  class Routes final : public SocialCalendarDispatchRead {
    const WorldBundle &w_;
    SocialCalendarDispatchFunction read_;

  public:
    Routes(const WorldBundle &w, SocialCalendarDispatchFunction read)
        : w_(w), read_(read) {}
    Outcome<SocialCalendarDispatch>
    dispatch(const StableId &actor) const override {
      return read_ ? read_(w_, actor)
                   : Outcome<SocialCalendarDispatch>{true, {}, {}};
    }
  } routes(w, selected_dispatch);
  // 同一实际 Calendar
  // 调用借用唯一完整候选持者，直到原事件及关系写入结束才释放。
  std::unique_ptr<CompleteExactWorldBindingOwner> relation_owner;
  OriginalSocialCalendarRelationWrite *relations = nullptr;
  if (w.social.original_relation_version == 2) {
    auto lease = w.complete_exact_time_bindings();
    if (!lease.ok)
      return Status{false, {}, lease.error};
    relation_owner = std::move(lease.value);
    if (!relation_owner)
      return common_failure("SocialCalendarOriginalOwningLeaseRequired",
                            p.period_id, ErrorKind::unsupported_capability);
    relations = relation_owner->original_social_calendar_relation_writer();
    if (!relations)
      return common_failure("OriginalSocialCalendarRelationWriterRequired",
                            p.period_id, ErrorKind::unsupported_capability);
    if (!relations->bound_to(w.common, w.social))
      return common_failure("OriginalSocialCalendarCandidateMismatch",
                            p.period_id, ErrorKind::internal);
    auto current = w.actual_phase_moment();
    if (!current.ok)
      return Status{false, {}, current.error};
    auto same = compare_world_moments(current.value, now.value);
    if (!same.ok)
      return Status{false, {}, same.error};
    if (same.value)
      return common_failure("OriginalSocialCalendarActualMomentMismatch",
                            p.period_id, ErrorKind::internal);
  }
  auto stepped = step_social_at_period(
      w.common, w.social, *w.catalog->social, *w.catalog->common, w.services,
      &w.services, p, now.value, &routes, relations);
  return stepped.ok ? common_success() : Status{false, {}, stepped.error};
}
ExactPhaseRegistration make_social_exact_phase_registration(
    Integer initial_tick, SocialCalendarDispatchFunction selected_dispatch) {
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::social;
  r.profile_id = "social_knowledge_time2";
  r.version = 1;
  r.stages = {ExactStage::social_exchange, ExactStage::due_message_receipt,
              ExactStage::knowledge_reaction_close};
  r.calendar_stages = {ExactStage::social_exchange,
                       ExactStage::knowledge_reaction_close};
  r.effect_writer_keys = {"social_macro_message_delivery",
                          "social_exact_report_collection",
                          "social_calendar_exchange_share_memory",
                          "original_economic_supply_projection"};
  r.period_source_owner = true;
  r.schedule_kind = ExactScheduleKind::exact_due;
  r.validate_author_selection = [initial_tick](const WorldBundle &w) {
    return validate_original_social_phase_selection(w, initial_tick);
  };
  r.validate_world_sources = [initial_tick](const WorldBundle &w,
                                            const WorldMoment &now) {
    return validate_original_social_phase_sources(w, now, initial_tick);
  };
  r.predict = [](const WorldBundle &w, const WorldMoment &from,
                 const WorldMoment &through)
      -> Outcome<std::vector<ExactEventPrediction>> {
    auto s = selected(w);
    if (!s.ok)
      return {false, {}, s.error};
    std::vector<ExactEventPrediction> out;
    const auto *t = w.world_timeline ? &*w.world_timeline : nullptr;
    const auto *o =
        t ? w.social_period_sources->openings.find(t->period_id) : nullptr;
    if (o && !w.social_period_sources->closures.find(o->period_id)) {
      WorldMoment due{o->through_tick, {}};
      auto a = compare_world_moments(from, due),
           b = compare_world_moments(due, through);
      if (!a.ok)
        return {false, {}, a.error};
      if (!b.ok)
        return {false, {}, b.error};
      if (a.value > 0)
        return bad<std::vector<ExactEventPrediction>>("OverdueSocialPeriod",
                                                      o->period_id);
      if (b.value <= 0)
        out.push_back({due,
                       ExactStage::due_message_receipt,
                       {"social", "social_period_open", o->period_id},
                       1,
                       0});
    }
    const auto &facts = w.common.state().facts;
    const auto &cursor = w.social.dynamic->time2->collection_cursor;
    if (!facts.empty() &&
        cursor.last_scanned_fact_id !=
            std::optional<StableId>{facts.rows().back().first}) {
      // schema 1 here is the existing Common FactRecord header, not an invented
      // owner payload schema.
      const auto &last = facts.rows().back().second;
      out.push_back({from,
                     ExactStage::knowledge_reaction_close,
                     {last.domain, last.kind, last.id},
                     1,
                     0});
    }
    return {true, std::move(out), {}};
  };
  r.apply_due = [](WorldBundle &w, const ExactEventPrediction &p,
                   ExactPhaseWriteContext &) -> Outcome<ExactEventApplied> {
    auto s = selected(w);
    if (!s.ok)
      return {false, {}, s.error};
    if (p.source_schema != 1)
      return bad<ExactEventApplied>("SocialDueSourceMismatch",
                                    p.actual_source_basis.id);
    if (p.stage == ExactStage::due_message_receipt) {
      if (p.actual_source_basis.domain != "social" ||
          p.actual_source_basis.kind != "social_period_open" ||
          p.actual_source_basis.id != w.world_timeline->period_id)
        return bad<ExactEventApplied>("SocialDueSourceMismatch",
                                      p.actual_source_basis.id);
      s = write_leaf(w, [&] {
        return deliver_social_due_period(
            w.common, w.social, *w.social_period_sources,
            p.actual_source_basis.id, p.at, w.catalog->identity);
      });
      if (!s.ok)
        return {false, {}, s.error};
      if (w.director.subject2) {
        auto source = capture_exact_result_time_read(w);
        if (!source.ok)
          return {false, {}, source.error};
        s = collect_director_knowledge_wakes2(w.common, w.social, *source.value,
                                              source.value->stamp(), w.director,
                                              p.at);
        if (!s.ok)
          return {false, {}, s.error};
        s = collect_director_task_commitment_wakes2(
            w.common, w.tasks, w.affairs, w.director, p.at);
        if (!s.ok)
          return {false, {}, s.error};
      }
      return {true,
              {{"social", "social_period_close", p.actual_source_basis.id}, 1},
              {}};
    }
    if (p.stage != ExactStage::knowledge_reaction_close)
      return bad<ExactEventApplied>("InvalidSocialStage");
    const auto &facts = w.common.state().facts;
    if (facts.empty() || facts.rows().back().first != p.actual_source_basis.id)
      return bad<ExactEventApplied>("SocialCollectionInputChanged");
    const auto &h = facts.rows().back().second;
    if (h.kind != p.actual_source_basis.kind ||
        h.domain != p.actual_source_basis.domain)
      return bad<ExactEventApplied>("SocialCollectionInputChanged");
    s = collect_reaction(w, p.at);
    if (!s.ok)
      return {false, {}, s.error};
    return {true,
            {{"social", "collection_cursor", p.actual_source_basis.id}, 1},
            {}};
  };
  r.apply_calendar_stage = [selected_dispatch](WorldBundle &w, ExactStage stage,
                                               const ExactPeriodContext &p,
                                               ExactPhaseWriteContext &writes) {
    return apply_original_social_calendar_stage(w, stage, p, writes,
                                                selected_dispatch);
  };
  r.open_period =
      [](WorldBundle &w, const ExactPeriodContext &p,
         ExactPhaseWriteContext &) -> Outcome<ExactPeriodOwnerSources> {
    auto s = selected(w);
    if (!s.ok)
      return {false, {}, s.error};
    s = open_social_due_period(w.common, w.social, *w.social_period_sources,
                               p.period_id, p.opening_at, w.catalog->identity);
    if (!s.ok)
      return {false, {}, s.error};
    return period_sources(w, p);
  };
  r.close_period =
      [](WorldBundle &w, const ExactPeriodContext &p,
         ExactPhaseWriteContext &) -> Outcome<ExactPeriodOwnerSources> {
    auto s = selected(w);
    if (!s.ok)
      return {false, {}, s.error};
    s = close_social_due_period(w.common, w.social, *w.social_period_sources,
                                p.period_id, {p.through_tick, {}},
                                w.catalog->identity);
    if (!s.ok)
      return {false, {}, s.error};
    return period_sources(w, p);
  };
  r.read_period_sources = period_sources;
  r.validate_applied_source = [](const WorldBundle &w,
                                 const ExactEventApplied &a) {
    auto s = selected(w);
    if (!s.ok)
      return s;
    if (a.source_schema != 1 || a.applied_result.domain != "social")
      return common_failure("InvalidSocialAppliedSource");
    if (a.applied_result.kind == "collection_cursor") {
      const auto &c = w.social.dynamic->time2->collection_cursor;
      return c.last_scanned_fact_id ==
                     std::optional<StableId>{a.applied_result.id}
                 ? common_success()
                 : common_failure("SocialCollectionSourceMissing");
    }
    if (a.applied_result.kind == "social_period_close") {
      const auto *o =
          w.social_period_sources->openings.find(a.applied_result.id);
      const auto *c =
          w.social_period_sources->closures.find(a.applied_result.id);
      if (!o || !c || c->delivered_message_ids != o->due_message_ids ||
          c->closed_at.half_day_tick != o->through_tick)
        return common_failure("SocialClosingSourceMissing");
      return common_success();
    }
    return common_failure("InvalidSocialAppliedSource");
  };
  return r;
}
} // namespace hanmo::resident
