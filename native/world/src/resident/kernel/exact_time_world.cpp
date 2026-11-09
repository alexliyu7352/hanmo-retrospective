// 唯一原WorldBundle在原submit/run里推进；本首profile只实际rest和显式未选宏观供给。
#include "exact_time_world.hpp"
#include "exact_complete_world.hpp"
#include "exact_rest_binding.hpp"
#include "world_bundle.hpp"
#include "exact_social_period.hpp"
#include "fixed_service_history.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
// 原精确错误逐项传播；没有用double或旧remaining替scheduler。
template<class T>Outcome<T> fail(const char* code){return {false,{},common_failure(code).error};}
// 仅供三条精确编排入口取得原注册图；构造短票先退出，后续专业回调必须重建自己的当前票。
// 不在普通read factory内返回失效票，不抢占外来active phase，也不为缺失票根造替身。
Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>>
bootstrap_complete_entry_bindings(const WorldBundle &world,
                                  const char *phase_name) {
  if (!world.read_stamps)
    return {false,
            {},
            common_failure("CompleteEntryReadStampRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto at = world.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  auto phase = world.enter_phase_read_context(phase_name, "world_timeline",
                                              {at.value, at.value}, at.value);
  if (!phase.ok)
    return {false, {}, std::move(phase.error)};
  auto original = world.complete_exact_time_bindings();
  if (!original.ok)
    return {false, {}, std::move(original.error)};
  if (!original.value)
    return {false,
            {},
            common_failure("CompleteEntryOriginalOwnerRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  return original;
}
} // namespace
// 纯预备请求上界与下一真实due；Ready允许预览，但实际正片须独立已完成Opening。
Outcome<WorldMoment> prepare_exact_advance_target(const WorldBundle& w,const std::optional<WorldMoment>& requested){
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    auto lease =
        bootstrap_complete_entry_bindings(w, "complete_prepare_binding");
    if (!lease.ok)
      return {false, {}, std::move(lease.error)};
    return prepare_complete_exact_advance_target(w, lease.value->reads(),
                                                 requested);
  }
 if(!w.catalog->exact_time||!w.world_timeline||!w.action_elapsed)return fail<WorldMoment>("ExactTimelinePairRequired");auto valid=validate_exact_rest_world_scope(w);if(!valid.ok)return {false,{},valid.error};
 auto current=read_world_moment(w.common.tick(),*w.world_timeline);if(!current.ok)return current;
 auto one=normalize_fraction("1","1");if(!one.ok)return {false,{},one.error};auto maximum=add_world_duration(current.value,HalfDayDuration{one.value});if(!maximum.ok)return maximum;
 WorldMoment target=requested?*requested:maximum.value;auto canonical=encode_world_moment(target);if(!canonical.ok)return {false,{},canonical.error};auto forward=compare_world_moments(current.value,target);if(!forward.ok)return {false,{},forward.error};auto bounded=compare_world_moments(target,maximum.value);if(!bounded.ok)return {false,{},bounded.error};if(forward.value>=0||bounded.value>0)return fail<WorldMoment>("InvalidExactAdvanceTarget");
 if(w.common.tick()==std::numeric_limits<Integer>::max())return fail<WorldMoment>("WorldTickOverflow");WorldMoment macro{w.common.tick()+1,{}};auto before=compare_world_moments(macro,target);if(!before.ok)return {false,{},before.error};if(before.value<0)target=macro;
 for(const auto& stored:w.action_elapsed->entries)if(const auto* fixed=std::get_if<ActionElapsedState>(&stored))if(const auto* running=std::get_if<ElapsedRunning>(&fixed->timing)){auto checked=validate_fixed_elapsed_value(*fixed,current.value,w.world_timeline->scale_ref);if(!checked.ok)return {false,{},checked.error};auto earliest=compare_world_moments(running->completion_due_at,target);if(!earliest.ok)return {false,{},earliest.error};if(earliest.value<0)target=running->completion_due_at;}
 return {true,std::move(target),{}};
}
Status open_exact_period_in_candidate(WorldBundle& w){
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    auto lease =
        bootstrap_complete_entry_bindings(w, "complete_opening_binding");
    if (!lease.ok)
      return {false, {}, std::move(lease.error)};
    return open_complete_exact_period_in_candidate(w, lease.value->reads());
  }
 auto valid=validate_exact_time_world(w);if(!valid.ok)return valid;if(w.world_timeline->boundary_state!=PeriodBoundaryState::ready)return common_failure("ExactPeriodAlreadyOpen");
 // provider清单确由独立作者输入显式为空且完整scope已核；不产生任何供给成功源。
 auto period=w.common.next_id("world_period");if(!period.ok)return {false,{},period.error};w.world_timeline->period_id=std::move(period.value);w.world_timeline->opening_receipt_ref.reset();w.world_timeline->close_receipt_ref.reset();w.world_timeline->boundary_state=PeriodBoundaryState::open;if(exact_social_selected(*w.catalog->exact_time)){auto now=read_world_moment(w.common.tick(),*w.world_timeline);if(!now.ok)return {false,{},now.error};auto opened=open_social_due_period(w.common,w.social,*w.social_period_sources,w.world_timeline->period_id,now.value,w.catalog->identity);if(!opened.ok)return opened;w.world_timeline->opening_receipt_ref=w.world_timeline->period_id;}return common_success();
}
Outcome<AdvanceDetails> advance_exact_world(WorldBundle& w,const std::optional<WorldMoment>& requested){
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    auto lease =
        bootstrap_complete_entry_bindings(w, "complete_advance_binding");
    if (!lease.ok)
      return {false, {}, std::move(lease.error)};
    return advance_complete_exact_world(w, lease.value->reads(), requested);
  }
 auto valid=validate_exact_time_world(w);if(!valid.ok)return {false,{},valid.error};if(w.world_timeline->boundary_state!=PeriodBoundaryState::open)return fail<AdvanceDetails>("ExactPeriodOpeningRequired");auto from=read_world_moment(w.common.tick(),*w.world_timeline);if(!from.ok)return {false,{},from.error};auto target=prepare_exact_advance_target(w,requested);if(!target.ok)return {false,{},target.error};auto span=make_moment_span(from.value,target.value);if(!span.ok)return {false,{},span.error};
 auto capture=w.common.begin_actor_condition_capture();if(!capture.ok)return {false,{},capture.error};std::vector<BoundAuthoredRestConditionWrite> conditions;AdvanceDetails result;
 auto ids=w.common.action_order();std::sort(ids.begin(),ids.end());for(const auto& id:ids){auto* entry=find_fixed_elapsed(*w.action_elapsed,id);const auto* original=w.common.action(id);if(!entry||!original)return fail<AdvanceDetails>("MissingExactActionBinding");if(!std::holds_alternative<ElapsedRunning>(entry->timing))return fail<AdvanceDetails>("UnsupportedExactRestTiming");
  auto slice=prepare_fixed_elapsed_slice(*entry,span.value,w.world_timeline->scale_ref);if(!slice.ok)return {false,{},slice.error};entry->credited_units=std::move(slice.value.credited_units);entry->last_settled_at=target.value;auto remaining=fixed_elapsed_remaining_ticks(*entry,w.world_timeline->scale_ref);if(!remaining.ok)return {false,{},remaining.error};auto action=*original;action.remaining_ticks=remaining.value;ActionMutation patch;patch.action=action;patch.remaining_ticks=remaining.value;auto applied=w.common.apply_action(patch);if(!applied.ok)return {false,{},applied.error};
  if(slice.value.due){auto finished=finish_exact_rest_in_candidate(w,action,target.value,conditions);if(!finished.ok)return {false,{},finished.error};result.completed_actions.insert(id,std::move(finished.value));auto released=release_action(w,id);if(!released.ok)return {false,{},released.error};}
 }
 // 最后正片/实际due都完成后才写唯一时钟；整数closing与规范化同一候选，下一Opening另发布。
 w.common.state().identity.tick=target.value.half_day_tick;w.world_timeline->sub_half_day=target.value.sub_half_day;auto whole=compare_fractions(target.value.sub_half_day,CanonicalFraction{});if(!whole.ok)return {false,{},whole.error};if(whole.value==0){w.world_timeline->boundary_state=PeriodBoundaryState::ready;w.world_timeline->close_receipt_ref.reset();if(exact_social_selected(*w.catalog->exact_time)){auto delivered=deliver_social_due_period(w.common,w.social,*w.social_period_sources,w.world_timeline->period_id,target.value,w.catalog->identity);if(!delivered.ok)return {false,{},delivered.error};auto closed=close_social_due_period(w.common,w.social,*w.social_period_sources,w.world_timeline->period_id,target.value,w.catalog->identity);if(!closed.ok)return {false,{},closed.error};w.world_timeline->close_receipt_ref=w.world_timeline->period_id;}}
 auto captured=validate_authored_rest_capture(w.common,*w.actor_foundation,conditions);if(!captured.ok)return {false,{},captured.error};auto closed=w.common.finish_actor_condition_capture();if(!closed.ok)return {false,{},closed.error};return {true,std::move(result),{}};
}
Status validate_exact_time_world(const WorldBundle &w) {
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    std::unique_ptr<CompleteExactWorldBindingOwner> lease;
    {
      // 真原图构造需要当前读票；该短票不可包住后续各 provider 的独立 phase。
      auto at = w.actual_phase_moment();
      if (!at.ok)
        return {false, {}, std::move(at.error)};
      auto phase = w.enter_phase_read_context("complete_validation_binding",
                                              "world_timeline",
                                              {at.value, at.value}, at.value);
      if (!phase.ok)
        return {false, {}, std::move(phase.error)};
      auto original = w.complete_exact_time_bindings();
      if (!original.ok)
        return {false, {}, std::move(original.error)};
      if (!original.value)
        return common_failure("CompleteValidationOriginalOwnerRequired", {},
                              ErrorKind::unsupported_capability);
      lease = std::move(original.value);
    }
    // 这里只借原注册/同根关联；原闭包在每个专业 phase 重建当前 owner，不复用 P1
    // 旧票。
    return validate_complete_exact_time_world(w, lease->reads());
  }
  if (!w.catalog->exact_time) {
    if (w.social_period_sources || w.world_timeline || w.action_elapsed ||
        w.actor_foundation || w.catalog->actor_foundation ||
        !w.growth.fixed_service_boundaries.empty())
      return common_failure("UnexpectedExactTimelineState");
    return common_success();
  }
  if (!w.world_timeline || !w.action_elapsed || !w.actor_foundation ||
      !w.catalog->actor_foundation)
    return common_failure("ExactTimelinePairRequired");
  auto scope = validate_exact_rest_world_scope(w);
  if (!scope.ok)
    return scope;
  const auto &expected = w.catalog->exact_time->scale_ref;
  const auto &actual = w.world_timeline->scale_ref;
  if (expected.definition_id != actual.definition_id ||
      expected.version != actual.version ||
      expected.units_per_half_day != actual.units_per_half_day)
    return common_failure("ExactTimelineScaleIdentityMismatch");
  auto timeline = validate_timeline_value(w.common.tick(), *w.world_timeline);
  if (!timeline.ok)
    return timeline;
  if (exact_social_selected(*w.catalog->exact_time)) {
    auto social = validate_exact_social_world(w);
    if (!social.ok)
      return social;
  } else if (w.world_timeline->opening_receipt_ref ||
             w.world_timeline->close_receipt_ref)
    return common_failure("UnselectedMacroReceipt");
  auto current = read_world_moment(w.common.tick(), *w.world_timeline);
  if (!current.ok)
    return {false, {}, current.error};
  auto history =
      validate_fixed_history_values(*w.action_elapsed, current.value, actual);
  if (!history.ok)
    return history;
  return validate_exact_rest_sources(w);
}
}
