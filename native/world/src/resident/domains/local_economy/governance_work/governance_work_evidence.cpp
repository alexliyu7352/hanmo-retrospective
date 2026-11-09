#include "governance_work_evidence.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}[[noreturn]]void fail(const StableId&code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
bool same(const WorkRef&a,const WorkRef&b){return a.owner_domain==b.owner_domain&&a.kind==b.kind&&a.work_id==b.work_id;}
bool same(const WorkAuthorizationRef&a,const WorkAuthorizationRef&b){return same_source_ref(a.program_ref,b.program_ref)&&a.target_line_id==b.target_line_id&&same_terms_revision(a.terms_revision,b.terms_revision)&&a.accepted_task_id==b.accepted_task_id;}
CanonicalFraction number(Integer n){return take(normalize_fraction(std::to_string(n),"1"));}
void equal_moment(const WorldMoment&a,const WorldMoment&b,const StableId&id){if(take(compare_world_moments(a,b)))fail("WorkFeePeriodWindowMismatch",id);}
CanonicalFraction economic_quantity(const EconomicWorkChargeReadContext&x,const WorkRef&work,const MomentSpan&span,const std::vector<MomentOwnerSource>&refs){
 auto read=take(read_economic_work_window(x.common,x.economy,x.catalog,x.state,x.inputs,work,span,x.common.state().identity));if(read.source_parts.size()!=refs.size())fail("WorkFeeEconomicSourcesMismatch",work.work_id);for(std::size_t i=0;i<refs.size();++i)if(refs[i].source_schema!=2||!same_source_ref(refs[i].source_ref,read.source_parts[i].receipt_ref))fail("WorkFeeEconomicSourcesMismatch",work.work_id);return read.actual_worker_halfdays;
}
}
// 只读取实际工作证据形成计费量；授权、期次或来源不符时按原规则拒绝。
Outcome<std::optional<WorkChargeMeasurement>>
read_original_s3_work_charge_evidence(const WorkExecutionRead &reader,
                                      const EconomicWorkChargeReadContext &x,
                                      const GovernanceWorkExpenseTerms &t,
                                      const WorkFeeLine &line,
                                      const WorkChargeEvidence &e) {
  try {
    if (std::holds_alternative<WorkOutcomeAppliedRef>(e) ||
        std::holds_alternative<WorkReportReceivedRef>(e))
      return {true, {}, {}};
    const auto *work = take(reader.read_work(t.work_id));
    if (!same(work->authorization, t.authorization) ||
        work->authorization.accepted_task_id != t.accepted_task_id)
      fail("WorkFeeAuthorizationMismatch", t.work_id);
    WorkChargeMeasurement out;
    out.work_id = t.work_id;
    out.canonical_scope_id = t.frozen.canonical_scope_id;
    out.trigger = line.trigger;
    out.unit = line.unit;
    if (const auto *ref = std::get_if<WorkActualStartRef>(&e)) {
      if (line.trigger != WorkFeeTrigger::actual_start ||
          line.unit != WorkFeeUnit::occurrence || ref->work_id != t.work_id ||
          ref->source_schema != 1)
        fail("WorkFeeStartRefMismatch", t.work_id);
      const auto *start =
          take(reader.read_actual_start(ref->work_id, ref->start_fact_id));
      if (!same(start->authorization, t.authorization) ||
          start->source_schema != ref->source_schema)
        fail("WorkFeeStartAuthorizationMismatch", t.work_id);
      out.canonical_occurrence = {"tasks", "work_actual_start",
                                  start->start_fact_id};
      out.occurred_at = start->started_at;
      out.actual_quantity = number(1);
      out.qualifies = true;
    } else if (const auto *ref = std::get_if<WorkCompletedRef>(&e)) {
      if (line.trigger != WorkFeeTrigger::work_completed ||
          line.unit != WorkFeeUnit::occurrence)
        fail("WorkFeeCompletionRefMismatch", t.work_id);
      const auto *result = take(reader.read_result_for_expense_evidence(
          ref->result_ref, ref->source_schema));
      if (result->work_id != t.work_id ||
          result->task_id != t.accepted_task_id ||
          !same(result->authorization, t.authorization))
        fail("WorkFeeCompletionScopeMismatch", t.work_id);
      out.canonical_occurrence = ref->result_ref;
      out.occurred_at = result->finished_at;
      out.qualifies = result->disposition == WorkResultDisposition::completed;
      out.actual_quantity = number(out.qualifies ? 1 : 0);
    } else if (const auto *measurement = std::get_if<WorkMeasuredRef>(&e)) {
      if (line.trigger != WorkFeeTrigger::measured_work ||
          line.unit == WorkFeeUnit::occurrence)
        fail("WorkFeeMeasurementUnitMismatch", t.work_id);
      if (const auto *ref = std::get_if<WorkMeasuredReceiptRef>(measurement)) {
        const auto *p = take(reader.read_period(ref->work_period_receipt_id));
        if (p->work_id != t.work_id)
          fail("WorkFeePeriodScopeMismatch", t.work_id);
        out.canonical_occurrence = {"tasks", "work_period_receipt", p->fact_id};
        out.occurred_at = p->actual_until;
        out.actual_window = MomentSpan{p->window.from, p->actual_until};
        if (line.unit == WorkFeeUnit::qualified_work_half_days) {
          out.actual_quantity = number(p->credited_half_days);
          out.qualifies = p->credited_half_days > 0;
        } else {
          const auto *source =
              std::get_if<WorkEconomicPeriodSource>(&p->source);
          const auto *plan =
              std::get_if<SupervisedWorkPlan>(&work->frozen_plan);
          if (!source || !plan ||
              !same(source->work_ref, plan->economic_work_ref))
            fail("WorkFeeEconomicWorkBindingRequired", t.work_id);
          equal_moment(source->window.from, p->window.from, t.work_id);
          equal_moment(source->window.to, p->window.to, t.work_id);
          out.actual_quantity =
              take(compare_world_moments(out.actual_window->from,
                                         out.actual_window->to)) == 0
                  ? CanonicalFraction{}
                  : economic_quantity(x, source->work_ref, *out.actual_window,
                                      source->source_parts);
          out.qualifies = take(compare_fractions(out.actual_quantity, {})) > 0;
        }
      } else {
        const auto &economic_measurement =
            std::get<WorkEconomicMeasurementRef>(*measurement);
        const auto *plan = std::get_if<SupervisedWorkPlan>(&work->frozen_plan);
        if (line.unit != WorkFeeUnit::economic_worker_half_days || !plan ||
            !same(economic_measurement.work_ref, plan->economic_work_ref) ||
            economic_measurement.actual_period_sources.empty())
          fail("WorkFeeEconomicWorkBindingRequired", t.work_id);
        // 精确窗口须来自实际采用的原工作期，拒绝调用者自选子窗口或同劳力重复计费。
        const WorkPeriodReceipt2 *adopted = nullptr;
        for (const auto &id : work->accepted_period_refs) {
          const auto *p = take(reader.read_period(id));
          if (take(compare_world_moments(
                  p->window.from, economic_measurement.window.from)) == 0 &&
              take(compare_world_moments(
                  p->actual_until, economic_measurement.window.to)) == 0) {
            if (adopted)
              fail("DuplicateWorkFeeAdoptedWindow", t.work_id);
            adopted = p;
          }
        }
        if (!adopted)
          fail("WorkFeeAdoptedWindowRequired", t.work_id);
        const auto *source =
            std::get_if<WorkEconomicPeriodSource>(&adopted->source);
        if (!source || !same(source->work_ref, economic_measurement.work_ref))
          fail("WorkFeeEconomicWorkBindingRequired", t.work_id);
        out.canonical_occurrence = {"tasks", "work_period_receipt",
                                    adopted->fact_id};
        out.occurred_at = adopted->actual_until;
        out.actual_window = economic_measurement.window;
        out.actual_quantity = economic_quantity(
            x, economic_measurement.work_ref, economic_measurement.window,
            economic_measurement.actual_period_sources);
        out.qualifies = take(compare_fractions(out.actual_quantity, {})) > 0;
      }
    } else
      fail("UnsupportedWorkChargeEvidence", t.work_id);
    return {true, std::move(out), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
}
