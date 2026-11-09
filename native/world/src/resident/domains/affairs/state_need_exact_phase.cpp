#include "resident/kernel/catalog_section_observation.hpp"
// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "state_need_exact_phase.hpp"
#include "affairs_calendar_phase.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/family/native_empty_care_sources.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 阶段只借本次候选的原期间与写上下文，不能以任意传入时刻充满意/到期屏障。
Status calendar_boundary(const WorldBundle &w, const ExactPeriodContext &p,
                         const ExactPhaseWriteContext &writes) {
  if (!w.world_timeline || p.period_id.empty() ||
      w.world_timeline->boundary_state != PeriodBoundaryState::open ||
      w.world_timeline->period_id != p.period_id ||
      w.active_exact_write_context() != &writes ||
      !w.common.actor_condition_capture_identity() ||
      p.opening_at.half_day_tick != w.common.tick() ||
      p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      p.through_tick != p.opening_at.half_day_tick + 1)
    return common_failure("StateNeedActualCalendarBoundaryRequired",
                          p.period_id, ErrorKind::internal);
  auto integral = compare_fractions(p.opening_at.sub_half_day, {});
  if (!integral.ok)
    return {false, {}, integral.error};
  if (integral.value)
    return common_failure("StateNeedActualCalendarBoundaryRequired",
                          p.period_id, ErrorKind::internal);
  auto actual = w.actual_phase_moment();
  if (!actual.ok)
    return {false, {}, actual.error};
  auto equal =
      compare_world_moments(actual.value, WorldMoment{p.through_tick, {}});
  if (!equal.ok)
    return {false, {}, equal.error};
  return equal.value ? common_failure("StateNeedActualCalendarMomentMismatch",
                                      p.period_id, ErrorKind::internal)
                     : common_success();
}
Status selected(const WorldBundle &w) {
  return w.world_timeline && w.affairs.state_needs &&
                 w.affairs.state_needs->exact_period_openings
             ? common_success()
             : common_failure("StateNeedOpeningProfile2Required");
}
Outcome<ExactPeriodOwnerSources> original_sources(const WorldBundle &w,
                                                  const ExactPeriodContext &p,
                                                  const StateNeedReadBinding &b,
                                                  bool closed) {
  auto valid = selected(w);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_state_need_period(*w.affairs.state_needs, p, closed,
                                     b.policies, b.tasks);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true,
          {{"affairs", "state_need_period_open", p.period_id},
           1,
           std::nullopt,
           std::nullopt},
          {}};
}
Outcome<bool> period_is_closed(const WorldBundle &w,
                               const ExactPeriodContext &p,
                               const WorldMoment &now) {
  auto through = compare_world_moments({p.through_tick, {}}, now);
  if (!through.ok)
    return {false, false, through.error};
  if (through.value > 0)
    return {true, false, {}};
  if (w.world_timeline->boundary_state == PeriodBoundaryState::ready)
    return {true, true, {}};
  return {true, w.world_timeline->period_id != p.period_id, {}};
}
} // namespace
Outcome<Integer>
read_state_need_selected_version(const CatalogIdentity &identity) {
  Integer selected = 0;
  for (const auto &m : identity.module_versions)
    if (m.first == "state_needs") {
      if (selected)
        return {false, {}, common_failure("DuplicateStateNeedsModule").error};
      if (m.second == "1")
        selected = 1;
      else if (m.second == "2")
        selected = 2;
      else
        return {
            false, {}, common_failure("UnsupportedStateNeedsVersion").error};
    }
  return {true, selected, {}};
}
Status validate_state_need_selected_version(const StateNeedState &s,
                                            const CatalogIdentity &identity,
                                            bool complete) {
  auto version = read_state_need_selected_version(identity);
  if (!version.ok)
    return {false, {}, version.error};
  if (!version.value || s.exact_period_openings != (version.value == 2))
    return common_failure("StateNeedSelectedVersionMismatch");
  if (complete != (version.value == 2))
    return common_failure("StateNeedExactPeriodProfileMismatch");
  if (version.value == 1 && !s.period_openings.empty())
    return common_failure("UnexpectedStateNeedPeriodOpenings");
  return common_success();
}
Status validate_state_need_catalog_absence(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 14, "PreparedCatalog", "validate_state_need_catalog_absence", &catalog);

  auto selected_version = read_state_need_selected_version(catalog.identity);
  if (!selected_version.ok)
    return {false, {}, selected_version.error};
  if (selected_version.value || !catalog.affairs || !catalog.tasks ||
      catalog.affairs->state_need_policies)
    return common_failure("StateNeedAbsenceCatalogMismatch", {},
                          ErrorKind::unsupported_capability);
  if (catalog.care_root_capability_selection ||
      (catalog.family_birth && catalog.family_birth->enabled) ||
      (catalog.family_care_relief && catalog.family_care_relief->enabled))
    return common_failure("StateNeedAbsenceDeferredCareSelected", {},
                          ErrorKind::unsupported_capability);
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "state_need_tasks" ||
        module.first == "state_task_originals" ||
        module.first == "care_requirements")
      return common_failure("StateTaskSelectionRequiresStateNeed", module.first,
                            ErrorKind::unsupported_capability);
  return common_success();
}
Status validate_state_need_world_absence(const WorldBundle &w) {
  if (!w.catalog)
    return common_failure("StateNeedActualCatalogRequired", {},
                          ErrorKind::internal);
  auto valid = validate_state_need_catalog_absence(*w.catalog);
  if (!valid.ok)
    return valid;
  // 未选根必须真正缺席，不能以已存在但当前为空的业务 owner 冒充未选择。
  if (w.affairs.state_needs || w.tasks.state_need_tasks)
    return common_failure("UnselectedStateNeedRootPresent", {},
                          ErrorKind::internal);
  for (const auto &row : w.common.state().facts.rows()) {
    const auto &fact = row.second;
    if (fact.kind.rfind("state_need", 0) == 0 ||
        fact.kind.rfind("state_claim", 0) == 0 ||
        fact.kind.rfind("state_task", 0) == 0 ||
        fact.kind == "predicate_contribution")
      return common_failure("UnselectedStateNeedOriginalFact", row.first,
                            ErrorKind::internal);
  }
  // Common 的成功结果及历史也是原保留来源，不能只清事实头后冒称空世界。
  const auto retained_kind = [](std::string_view kind) {
    return kind.starts_with("state_need") || kind.starts_with("state_claim") ||
           kind.starts_with("state_task") || kind == "predicate_contribution";
  };
  for (const auto &entry : w.common.state().history)
    if (retained_kind(entry.kind))
      return common_failure("UnselectedStateNeedHistory", entry.result_id,
                            ErrorKind::internal);
  for (const auto &entry : w.common.state().results.rows())
    if (entry.second && retained_kind(entry.second->kind))
      return common_failure("UnselectedStateNeedResult", entry.first,
                            ErrorKind::internal);
  for (const auto &row : w.common.state().actions.rows())
    if (row.second.kind.rfind("state_need", 0) == 0 ||
        row.second.kind.rfind("state_task", 0) == 0)
      return common_failure("UnselectedStateNeedAction", row.first,
                            ErrorKind::internal);
  for (const auto &row : w.affairs.affairs.rows())
    if (row.second.semantic_key.rfind("state:care:", 0) == 0 ||
        row.second.semantic_key.rfind("state:medical:", 0) == 0)
      return common_failure("UnselectedStateGoalAffair", row.first,
                            ErrorKind::internal);
  // 当前正式 World 没有 Care19/TravelDependent owning 槽；nullptr
  // 是真实未安装，
  // 不是替换已存在的空表。若将来安装这些根，中央必须把原指针加入同址装配。
  NativeEmptyCareSourceRead care(
      {w.common, &w.family_birth, nullptr,
       w.actor_foundation ? &*w.actor_foundation : nullptr,
       w.action_elapsed ? &*w.action_elapsed : nullptr, nullptr,
       &w.child_care_relief});
  return care.validate_sources();
}
ExactPhaseRegistration
make_state_need_deadline_phase_registration(StateNeedWorldFactory factory,
                                            Integer selected_version) {
  if (selected_version != 2) {
    ExactPhaseRegistration r;
    r.provider = ExactProviderId::affairs;
    r.profile_id = "state_need_deadline_time2";
    r.version = 2;
    r.stages = {ExactStage::economic_supply_affairs,
                ExactStage::state_satisfaction,
                ExactStage::state_need_deadline_close};
    r.calendar_stages = r.stages;
    // 真正未选 StateNeed 没有期间源，不能发空 SourceRef 或制造 opening。
    r.period_source_owner = false;
    r.effect_writer_keys = {"state_need_deadline_close",
                            "original_quantity_task_settlement",
                            "original_supply_affairs_refresh"};
    r.validate_author_selection = [selected_version](const WorldBundle &w) {
      if (selected_version != 0)
        return common_failure("UnsupportedCompleteStateNeedSelection", {},
                              ErrorKind::unsupported_capability);
      return validate_state_need_world_absence(w);
    };
    r.validate_world_sources = [selected_version](const WorldBundle &w,
                                                  const WorldMoment &now) {
      if (selected_version != 0)
        return common_failure("UnsupportedCompleteStateNeedSelection", {},
                              ErrorKind::unsupported_capability);
      auto valid = validate_state_need_world_absence(w);
      if (!valid.ok)
        return valid;
      auto actual = w.actual_phase_moment();
      if (!actual.ok)
        return Status{false, {}, actual.error};
      auto equal = compare_world_moments(actual.value, now);
      if (!equal.ok)
        return Status{false, {}, equal.error};
      if (equal.value)
        return common_failure("StateNeedAbsenceActualMomentMismatch", {},
                              ErrorKind::internal);
      return validate_original_affairs_calendar_sources(w, now);
    };
    r.apply_calendar_stage =
        [selected_version](WorldBundle &w, ExactStage stage,
                           const ExactPeriodContext &p,
                           ExactPhaseWriteContext &writes) {
          if (selected_version != 0)
            return common_failure("UnsupportedCompleteStateNeedSelection", {},
                                  ErrorKind::unsupported_capability);
          auto valid = validate_state_need_world_absence(w);
          if (!valid.ok)
            return valid;
          if (stage == ExactStage::economic_supply_affairs) {
            valid = step_original_affairs_calendar(w, p);
            if (!valid.ok)
              return valid;
            return validate_state_need_world_absence(w);
          }
          if (stage != ExactStage::state_satisfaction &&
              stage != ExactStage::state_need_deadline_close)
            return common_failure("InvalidStateNeedDeadlineStage");
          // actual0
          // 的满意阶段只证明没有此类需求，不发行满意事实或关闭普通任务。
          return calendar_boundary(w, p, writes);
        };
    return r;
  }

  ExactPhaseRegistration r;
  r.provider = ExactProviderId::affairs;
  r.profile_id = "state_need_deadline_time2";
  r.version = 2;
  r.stages = {ExactStage::economic_supply_affairs,
              ExactStage::state_satisfaction,
              ExactStage::state_need_deadline_close};
  r.calendar_stages = r.stages;
  r.period_source_owner = true;
  r.period_closure_kind = ExactPeriodClosureKind::stage_barrier;
  r.effect_writer_keys = {"state_need_deadline_close",
                          "original_quantity_task_settlement",
                          "original_supply_affairs_refresh"};
  r.validate_author_selection = [factory](const WorldBundle &w) {
    if (!factory.read || !factory.write)
      return common_failure("StateNeedOriginalFactoryRequired");
    if (!w.catalog)
      return common_failure("StateNeedActualCatalogRequired");
    auto version = read_state_need_selected_version(w.catalog->identity);
    if (!version.ok)
      return Status{false, {}, version.error};
    if (version.value != 2)
      return common_failure("StateNeedSelectedVersionMismatch");
    auto valid = selected(w);
    if (!valid.ok)
      return valid;
    return validate_state_need_selected_version(*w.affairs.state_needs,
                                                w.catalog->identity, true);
  };
  r.validate_world_sources = [factory](const WorldBundle &w,
                                       const WorldMoment &now) {
    auto valid = selected(w);
    if (!valid.ok)
      return valid;
    if (!factory.read)
      return common_failure("StateNeedOriginalFactoryRequired");
    auto lease = factory.read(w, now);
    if (!lease.ok)
      return Status{false, {}, lease.error};
    if (!lease.value)
      return common_failure("StateNeedReadLeaseMissing");
    auto b = lease.value->bindings();
    valid = validate_state_needs(w.common, w.affairs, b.goals, b.tasks, now,
                                 b.policies);
    if (!valid.ok)
      return valid;
    valid = validate_original_affairs_calendar_sources(w, now);
    if (!valid.ok)
      return valid;
    return validate_state_need_period_history(
        *w.affairs.state_needs, *w.world_timeline, now, b.policies, b.tasks);
  };
  r.open_period =
      [factory](WorldBundle &w, const ExactPeriodContext &p,
                ExactPhaseWriteContext &) -> Outcome<ExactPeriodOwnerSources> {
    auto valid = selected(w);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (!factory.read)
      return {
          false, {}, common_failure("StateNeedOriginalFactoryRequired").error};
    auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, now.error};
    auto equal = compare_world_moments(now.value, p.opening_at);
    if (!equal.ok)
      return {false, {}, equal.error};
    if (equal.value)
      return {
          false,
          {},
          common_failure("StateNeedOpeningMomentMismatch", p.period_id).error};
    auto lease = factory.read(w, now.value);
    if (!lease.ok)
      return {false, {}, lease.error};
    if (!lease.value)
      return {false, {}, common_failure("StateNeedReadLeaseMissing").error};
    auto b = lease.value->bindings();
    valid =
        open_state_need_period(*w.affairs.state_needs, p, b.policies, b.tasks);
    if (!valid.ok)
      return {false, {}, valid.error};
    return original_sources(w, p, b, false);
  };
  r.apply_calendar_stage = [factory](WorldBundle &w, ExactStage stage,
                                     const ExactPeriodContext &p,
                                     ExactPhaseWriteContext &writes) {
    auto valid = selected(w);
    if (!valid.ok)
      return valid;
    if (stage == ExactStage::economic_supply_affairs)
      return step_original_affairs_calendar(w, p);
    if (stage == ExactStage::state_satisfaction) {
      valid = calendar_boundary(w, p, writes);
      if (!valid.ok)
        return valid;
      if (!factory.read)
        return common_failure("StateNeedOriginalFactoryRequired");
      auto now = w.actual_phase_moment();
      if (!now.ok)
        return Status{false, {}, now.error};
      auto lease = factory.read(w, now.value);
      if (!lease.ok)
        return Status{false, {}, lease.error};
      if (!lease.value)
        return common_failure("StateNeedReadLeaseMissing");
      const auto b = lease.value->bindings();
      // 满意结果仍由实际 Care/Medical 原 owner 产生；这里只读其原 goal 与
      // claim。
      return validate_state_needs(w.common, w.affairs, b.goals, b.tasks,
                                  now.value, b.policies);
    }
    if (stage != ExactStage::state_need_deadline_close)
      return common_failure("InvalidStateNeedDeadlineStage");
    if (!factory.write)
      return common_failure("StateNeedOriginalFactoryRequired");
    auto now = w.actual_phase_moment();
    if (!now.ok)
      return Status{false, {}, now.error};
    auto lease = factory.write(w, now.value);
    if (!lease.ok)
      return Status{false, {}, lease.error};
    if (!lease.value)
      return common_failure("StateNeedWriteLeaseMissing");
    auto b = lease.value->bindings();
    return close_state_need_period(w.common, w.affairs, p, b.policies, b.goals,
                                   b.tasks, now.value);
  };
  r.close_period =
      [factory](WorldBundle &w, const ExactPeriodContext &p,
                ExactPhaseWriteContext &) -> Outcome<ExactPeriodOwnerSources> {
    auto valid = selected(w);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (!factory.read)
      return {
          false, {}, common_failure("StateNeedOriginalFactoryRequired").error};
    auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, now.error};
    auto equal = compare_world_moments(now.value, {p.through_tick, {}});
    if (!equal.ok)
      return {false, {}, equal.error};
    if (equal.value)
      return {
          false,
          {},
          common_failure("StateNeedClosingMomentMismatch", p.period_id).error};
    auto lease = factory.read(w, now.value);
    if (!lease.ok)
      return {false, {}, lease.error};
    if (!lease.value)
      return {false, {}, common_failure("StateNeedReadLeaseMissing").error};
    return original_sources(w, p, lease.value->bindings(), true);
  };
  r.read_period_sources =
      [factory](
          const WorldBundle &w,
          const ExactPeriodContext &p) -> Outcome<ExactPeriodOwnerSources> {
    auto valid = selected(w);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (!factory.read)
      return {
          false, {}, common_failure("StateNeedOriginalFactoryRequired").error};
    auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, now.error};
    auto lease = factory.read(w, now.value);
    if (!lease.ok)
      return {false, {}, lease.error};
    if (!lease.value)
      return {false, {}, common_failure("StateNeedReadLeaseMissing").error};
    auto closed = period_is_closed(w, p, now.value);
    if (!closed.ok)
      return {false, {}, closed.error};
    return original_sources(w, p, lease.value->bindings(), closed.value);
  };
  return r;
}
} // namespace hanmo::resident
