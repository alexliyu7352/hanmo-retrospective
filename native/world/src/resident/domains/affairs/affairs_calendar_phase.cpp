#include "affairs_calendar_phase.hpp"
#include "affairs_model.hpp"
#include "issued_supply/affairs_issued_supply.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "resident/domains/local_economy/economic_debt_needs.hpp"
#include "resident/kernel/political_takeover_execution_world.hpp"
#include "resident/kernel/economic_supply_policy_integration.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_need_adapter.hpp"
#include "resident/domains/tasks/task_personal_advance_rules.hpp"
#include <limits>
namespace hanmo::resident {namespace {
Status selected(const WorldBundle&w){return w.dynamic_enabled()&&w.catalog->affairs&&w.catalog->tasks&&w.catalog->local_economy&&w.world_timeline?common_success():common_failure("OriginalAffairsCalendarProfileRequired");}
Outcome<DynamicStepInput> calendar_input(const WorldBundle&w,const ExactPeriodContext&p){auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto valid=encode_world_moment(p.opening_at);if(!valid.ok)return {false,{},valid.error};auto whole=compare_world_moments(p.opening_at,{p.opening_at.half_day_tick,{}});if(!whole.ok)return {false,{},whole.error};auto due=compare_world_moments(now.value,{p.through_tick,{}});if(!due.ok)return {false,{},due.error};if(whole.value||due.value||p.period_id.empty()||p.opening_at.half_day_tick==std::numeric_limits<Integer>::max()||p.through_tick!=p.opening_at.half_day_tick+1||w.common.tick()!=p.opening_at.half_day_tick||w.world_timeline->period_id!=p.period_id||w.world_timeline->boundary_state!=PeriodBoundaryState::open)return {false,{},common_failure("OriginalAffairsCalendarPeriodMismatch",p.period_id).error};DynamicStepInput input;input.from_tick=p.opening_at.half_day_tick;input.to_tick=p.through_tick;return {true,std::move(input),{}};}
// This is only the original integer-calendar read adapter. Its empty ID vectors
// are never an Opening membership sample and are not used to grant old service.
Outcome<std::optional<DynamicStepInput>> read_phase(const WorldBundle&w,const WorldMoment&now){auto valid=encode_world_moment(now);if(!valid.ok)return {false,{},valid.error};if(now.half_day_tick==w.common.tick())return {true,std::nullopt,{}};if(w.common.tick()==std::numeric_limits<Integer>::max()||now.half_day_tick!=w.common.tick()+1)return {false,{},common_failure("OriginalAffairsReadMomentMismatch").error};auto equal=compare_world_moments(now,{now.half_day_tick,{}});if(!equal.ok)return {false,{},equal.error};if(equal.value)return {false,{},common_failure("OriginalAffairsReadMomentMismatch").error};DynamicStepInput input;input.from_tick=w.common.tick();input.to_tick=now.half_day_tick;return {true,std::move(input),{}};}
}
// 原整数日历顺序不变；只在已启用供给政策确实需要 phase 刷新时明确拒绝缺失实现。
Status step_original_affairs_calendar(WorldBundle &w,
                                      const ExactPeriodContext &p) {
  auto valid = selected(w);
  if (!valid.ok)
    return valid;
  auto phase = calendar_input(w, p);
  if (!phase.ok)
    return {false, {}, phase.error};
  const auto &input = phase.value;
  WorldCurrentMomentRead clock(w);
  {
    auto access = PoliticalTakeoverExecutionWorldAccess::create(w, &input);
    if (!access.ok)
      return {false, {}, access.error};
    EconomicDebtNeedClosingContext debt{
        w.local_economy, *w.catalog->local_economy, clock, {}, {}};
    if (w.catalog->affairs->fiscal_need_policies) {
      if (!w.fiscal_settlement_enabled())
        return common_failure("FiscalNeedsRequireSettlement");
      debt.fiscal.emplace(FiscalDebtNeedSources{
          w.fiscal_settlement, *w.catalog->fiscal_settlement,
          *w.catalog->affairs->fiscal_need_policies,
          w.occupation_fiscal_needs_enabled() ? &w.occupation_fiscal_settlement
                                              : nullptr});
    }
    if (w.catalog->affairs->cohort_wage_need_policies)
      debt.cohort_wages.emplace(CohortDebtNeedSources{
          *w.catalog->affairs->cohort_wage_need_policies,
          access.value->economy(),
          access.value->economy() ? &w.economic_takeover_execution : nullptr});
    valid = reconcile_economic_debt_needs_at_calendar_boundary_in_candidate(
        w.common, w.affairs, debt, p);
    if (!valid.ok)
      return valid;
  }
  {
    auto access = PoliticalTakeoverExecutionWorldAccess::create(w, &input);
    if (!access.ok)
      return {false, {}, access.error};
    std::optional<TaskQuantityDeadlineContext> deadline;
    if (w.tasks.exact_quantities) {
      if (!w.social_period_sources)
        return common_failure("QuantityDeadlineSocialOpeningOwnerRequired", {},
                              ErrorKind::unsupported_capability);
      deadline.emplace(TaskQuantityDeadlineContext{w.common, w.tasks,
                                                   *w.social_period_sources,
                                                   w.catalog->identity, clock});
    }
    valid = reconcile_task_contracts(
        w.common, w.tasks, w.affairs, input.to_tick, access.value->tasks(),
        &input, deadline ? &*deadline : nullptr, &p);
    if (!valid.ok)
      return valid;
    if (deadline) {
      valid = validate_task_quantity_deadline_sources(*deadline);
      if (!valid.ok)
        return valid;
    }
  }
  if (w.catalog->task_personal_advances) {
    if (!w.tasks.personal_advances)
      return common_failure("MissingTaskAdvanceDomain");
    for (const auto &row : w.tasks.personal_advances->terms.rows()) {
      const auto *c = w.tasks.contracts.find(row.first);
      if (!c)
        return common_failure("TaskAdvanceSourceMismatch", row.first);
      if (!c->execution_stopped || c->status == ContractStatus::settled)
        continue;
      auto repayment = repay_task_personal_advance_in_candidate(
          w.common, w.tasks, *w.tasks.personal_advances, &w.organization,
          row.first, &input);
      if (!repayment.ok)
        return {false, {}, repayment.error};
      if (repayment.value.kind == TaskAdvanceRepaymentKind::pending)
        continue;
      auto access = PoliticalTakeoverExecutionWorldAccess::create(w, &input);
      if (!access.ok)
        return {false, {}, access.error};
      valid = settle_task_contract(w.common, w.tasks, row.first, input.to_tick,
                                   access.value->tasks(), &input);
      if (!valid.ok)
        return valid;
    }
  }
  auto observed = [&]() -> Outcome<std::vector<SupplyObservation>> {
    auto access = PoliticalTakeoverExecutionWorldAccess::create(w, &input);
    if (!access.ok)
      return {false, {}, access.error};
    return refresh_local_supply_observations(
        w.common, w.local_economy, *w.catalog->local_economy, input.to_tick,
        access.value->economy());
  }();
  if (!observed.ok)
    return {false, {}, observed.error};
  // 现有供给观察写者只认 committed clock；不得丢弃 phase 冒充当前边界。
  if (w.economic_supply_policies_enabled()) {
    auto access = EconomicSupplyWorldAccess::create(w, &input);
    if (!access.ok)
      return {false, {}, access.error};
    auto failure = common_failure("NOT_IMPLEMENTED",
                                  "EconomicSupplyPhaseClockRefreshRequired",
                                  ErrorKind::unsupported_capability);
    failure.error.reason = "EconomicSupplyPhaseClockRefreshRequired";
    return failure;
  }
  {
    auto access = PoliticalTakeoverExecutionWorldAccess::create(w, &input);
    if (!access.ok)
      return {false, {}, access.error};
    auto suspensions = access.value->supply_suspensions();
    if (!suspensions.ok)
      return {false, {}, suspensions.error};
    if (access.value->issued_supply())
      valid = reconcile_affairs_with_issued_supply(
          w.common, w.affairs, *w.catalog->affairs, w.tasks, *w.catalog->tasks,
          observed.value, w.services, input.to_tick,
          *access.value->issued_supply(), access.value->tasks(),
          &suspensions.value, &input);
    else
      valid = reconcile_affairs(w.common, w.affairs, *w.catalog->affairs,
                                w.tasks, observed.value, w.services,
                                input.to_tick, access.value->tasks(),
                                &suspensions.value, nullptr, &input);
    if (!valid.ok)
      return valid;
  }
  {
    auto access = PoliticalTakeoverExecutionWorldAccess::create(w, &input);
    if (!access.ok)
      return {false, {}, access.error};
    if (access.value->issued_supply())
      valid = publish_need_offers_with_issued_supply(
          w.common, w.affairs, *w.catalog->affairs, w.tasks, *w.catalog->tasks,
          input.to_tick, *access.value->issued_supply(), &input);
    else
      valid = publish_need_offers(w.common, w.affairs, *w.catalog->affairs,
                                  w.tasks, *w.catalog->tasks, input.to_tick);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
Status validate_original_affairs_calendar_sources(const WorldBundle&w,const WorldMoment&now){auto valid=selected(w);if(!valid.ok)return valid;auto phase=read_phase(w,now);if(!phase.ok)return {false,{},phase.error};const auto*input=phase.value?&*phase.value:nullptr;auto access=PoliticalTakeoverExecutionWorldAccess::create(w,input);if(!access.ok)return {false,{},access.error};auto suspensions=access.value->supply_suspensions();if(!suspensions.ok)return {false,{},suspensions.error};
 WorldCurrentMomentRead clock(w);if(w.tasks.exact_quantities){if(!w.social_period_sources)return common_failure("QuantityDeadlineSocialOpeningOwnerRequired",{},ErrorKind::unsupported_capability);auto deadlines=validate_task_quantity_deadline_sources({w.common,w.tasks,*w.social_period_sources,w.catalog->identity,clock});if(!deadlines.ok)return deadlines;}std::optional<FiscalNeedAccess> fiscal;if(w.catalog->affairs->fiscal_need_policies){if(!w.fiscal_settlement_enabled())return common_failure("FiscalNeedsRequireSettlement");if(w.occupation_fiscal_needs_enabled())fiscal.emplace(w.local_economy,w.fiscal_settlement,*w.catalog->fiscal_settlement,*w.catalog->affairs->fiscal_need_policies,w.occupation_fiscal_settlement,&clock);else fiscal.emplace(w.local_economy,w.fiscal_settlement,*w.catalog->fiscal_settlement,*w.catalog->affairs->fiscal_need_policies,&clock);}
 std::optional<CohortWageNeedAccess>wages;if(w.catalog->affairs->cohort_wage_need_policies)wages.emplace(w.local_economy,*w.catalog->local_economy,*w.catalog->affairs->cohort_wage_need_policies,access.value->economy(),access.value->economy()?&w.economic_takeover_execution:nullptr,&clock);
 if(access.value->issued_supply())return validate_affairs_with_issued_supply(w.common,w.affairs,*w.catalog->affairs,w.tasks,*w.catalog->tasks,*access.value->issued_supply(),&w.services,fiscal?&*fiscal:nullptr,wages?&*wages:nullptr,&suspensions.value,input,&now);
 return validate_affairs(w.common,w.affairs,*w.catalog->affairs,w.tasks,&w.services,fiscal?&*fiscal:nullptr,wages?&*wages:nullptr,&suspensions.value,nullptr,&now);
}
}
