#include "cargo_handling_phase.hpp"
#include "movement_exact_detail.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool active(const CargoHandlingExecution& e){return e.phase==CargoHandlingPhase::working||e.phase==CargoHandlingPhase::suspended;}
bool terminal(const CargoHandlingExecution& e){return e.terminal_result_ref.has_value();}
std::shared_ptr<const ReadStamp> current_stamp(const CargoHandlingReadContext& c,const CargoHandlingTerms& t,const CargoHandlingExecution& e){std::vector<ResourceKey> resources{{ResourceKey::Kind::inventory,t.source_inventory_id},{ResourceKey::Kind::inventory,t.destination_inventory_id}};if(const auto* named=std::get_if<NamedCargoHandlingWorkers>(&t.labor_selection))for(const auto& id:named->participant_actor_ids)resources.push_back({ResourceKey::Kind::actor,id});return take(c.stamps.capture(resources,{{"travel","cargo_handling_terms",t.terms_id},{"travel","cargo_handling_execution",e.execution_id}},{t.business_source.id}));}
CargoHandlingReadContext read_context(const CargoHandlingWriteContext& c){return {c.catalog,c.scale,c.business,c.labor,c.facility,c.stamps,c.budget};}
void add_completion(ExactPhaseWriteContext& context,const CargoHandlingExecution& e){if(const auto* named=std::get_if<NamedCargoHandlingTiming>(&e.timing)){ActionCompletion completed;completed.completed=true;if(!context.advance.completed_actions.insert(named->action_ref,std::move(completed)))fail("DuplicateCargoHandlingActionCompletion",named->action_ref);}}
}
// 预测为闭区间，等点仍须枚举当前真实due；只拒反向，不放宽正片writer。
Outcome<std::vector<ExactEventPrediction>> predict_cargo_handling_events(
    const CargoHandlingState &s, const ActionElapsedStore &elapsed,
    const CargoHandlingReadContext &c, const WorldMoment &current,
    const WorldMoment &upper) {
  return guard<std::vector<ExactEventPrediction>>([&] {
    if (take(compare_world_moments(current, upper, c.budget)) > 0)
      take(make_moment_span(current, upper, c.budget));
    std::vector<ExactEventPrediction> out;
    for (const auto &row : s.executions.rows()) {
      const auto &e = row.second;
      if (terminal(e))
        continue;
      const auto *t = s.terms.find(e.terms_ref);
      if (!t)
        fail("CargoHandlingTermsRequired", e.execution_id);
      auto stamp = current_stamp(c, *t, e);
      auto ending = take(
          c.business.read_pending_handling_termination(*t, e, current, *stamp));
      if (ending) {
        if (ending->status == CargoHandlingResultStatus::completed ||
            !source_valid(ending->actual_cause) ||
            !same_moment(ending->at, current, c.budget))
          fail("CargoHandlingActualTerminationRequired", e.execution_id);
        out.push_back({current,
                       ExactStage::immediate_consequences,
                       {"travel", "cargo_handling_execution", e.execution_id},
                       1,
                       e.progress_source_revision});
        continue;
      }
      if (active(e)) {
        stamp = current_stamp(c, *t, e);
        auto change = take(
            c.labor.read_pending_handling_labor_change(*t, e, current, *stamp));
        if (change) {
          if (!source_valid(change->actual_cause) ||
              !same_moment(change->at, current, c.budget) ||
              (change->resume && e.phase != CargoHandlingPhase::suspended) ||
              (!change->resume && e.phase != CargoHandlingPhase::working))
            fail("CargoHandlingActualLaborChangeRequired", e.execution_id);
          out.push_back({current,
                         ExactStage::immediate_consequences,
                         {"travel", "cargo_handling_execution", e.execution_id},
                         1,
                         e.progress_source_revision});
          continue;
        }
        auto due = take(predict_cargo_handling_service_due(
            s, elapsed, e.execution_id, c.scale, c.budget));
        if (due) {
          if (take(compare_world_moments(*due, current, c.budget)) < 0)
            fail("CargoHandlingUnsettledServiceGate", e.execution_id);
          if (take(compare_world_moments(*due, upper, c.budget)) <= 0)
            out.push_back(
                {*due,
                 ExactStage::action_completions,
                 {"travel", "cargo_handling_execution", e.execution_id},
                 1,
                 e.progress_source_revision});
        }
      } else if (e.phase == CargoHandlingPhase::ready_to_transfer) {
        const auto *work = e.work_result_ref
                               ? s.work_results.find(*e.work_result_ref)
                               : nullptr;
        if (!work)
          fail("CargoHandlingWorkResultRequired", e.execution_id);
        stamp = current_stamp(c, *t, e);
        auto disposition = take(c.business.read_actual_transfer_disposition(
            *t, *work, current, *stamp));
        if (std::holds_alternative<CargoHandlingTransferReady>(disposition))
          out.push_back(
              {current,
               ExactStage::action_completions,
               {"travel", "cargo_handling_work_result", work->result_id},
               1,
               e.progress_source_revision});
        else {
          const auto &waiting =
              std::get<CargoHandlingTransferWait>(disposition);
          if (!source_valid(waiting.actual_waiting_source) ||
              take(compare_world_moments(
                  take(c.business.read_actual_handling_cause_moment(
                      waiting.actual_waiting_source)),
                  current, c.budget)) > 0)
            fail("CargoHandlingActualWaitSourceRequired", e.execution_id);
        }
      } else
        fail("CargoHandlingUnprovenPhase", e.execution_id);
    }
    return out;
  });
}
Status provide_cargo_handling_interval(CommonStore& common,CargoHandlingState& state,ActionElapsedStore& elapsed,const CargoHandlingWriteContext& c,const MomentSpan& span){return status([&]{std::vector<StableId> ids;for(const auto& row:state.executions.rows())if(active(row.second))ids.push_back(row.first);for(const auto& id:ids)take(advance_cargo_handling_in_candidate(common,state,elapsed,c,id,span));});}
Outcome<ExactEventApplied> apply_cargo_handling_event(CommonStore& common,CargoHandlingState& state,ActionElapsedStore& elapsed,const CargoHandlingWriteContext& c,const ExactEventPrediction& event,ExactPhaseWriteContext& context){return guard<ExactEventApplied>([&]{const auto& ref=event.actual_source_basis;if(ref.domain!="travel"||event.source_schema!=1)fail("CargoHandlingEventSourceRequired",ref.id);const CargoHandlingWorkResult* work=ref.kind=="cargo_handling_work_result"?state.work_results.find(ref.id):nullptr;const StableId id=work?work->execution_ref:ref.id;auto* e=state.executions.find(id);const auto* t=e?state.terms.find(e->terms_ref):nullptr;if(!e||!t||terminal(*e)||event.dependency_revision!=e->progress_source_revision)fail("StaleCargoHandlingPrediction",ref.id);auto read=read_context(c);if(event.stage==ExactStage::immediate_consequences){if(ref.kind!="cargo_handling_execution")fail("CargoHandlingEventKindMismatch",id);auto stamp=current_stamp(read,*t,*e);auto ending=take(c.business.read_pending_handling_termination(*t,*e,event.at,*stamp));if(ending){if(!source_valid(ending->actual_cause)||!same_moment(ending->at,event.at,c.budget))fail("StaleCargoHandlingTermination",id);auto result=take(terminate_cargo_handling_in_candidate(common,state,elapsed,c,id,ending->status,ending->actual_cause,event.at));return ExactEventApplied{{"travel","cargo_handling_result",result},1};}stamp=current_stamp(read,*t,*e);auto change=take(c.labor.read_pending_handling_labor_change(*t,*e,event.at,*stamp));if(!change||!source_valid(change->actual_cause)||!same_moment(change->at,event.at,c.budget))fail("StaleCargoHandlingLaborChange",id);take(change_cargo_handling_service_in_candidate(common,state,elapsed,c,id,change->resume,change->actual_cause,event.at));return ExactEventApplied{{"travel","cargo_handling_execution",id},1};}
if(event.stage!=ExactStage::action_completions)fail("CargoHandlingEventStageMismatch",id);if(ref.kind=="cargo_handling_execution"){const auto result=take(complete_cargo_handling_service_in_candidate(common,state,elapsed,c,id,event.at));add_completion(context,*state.executions.find(id));return ExactEventApplied{{"travel","cargo_handling_work_result",result},1};}if(ref.kind=="cargo_handling_work_result"){auto result=take(complete_cargo_handling_transfer_in_candidate(common,state,c,id,event.at));if(!result)fail("CargoHandlingPredictionChangedWithoutWrite",id);return ExactEventApplied{{"travel","cargo_handling_result",*result},1};}fail("CargoHandlingEventKindMismatch",id);});}
// 原劳力/业务producer写后在同候选立即调用；不生成跨phase回边事件。
Outcome<std::vector<ExactEventApplied>> apply_pending_cargo_handling_consequences_in_candidate(CommonStore& common,CargoHandlingState& state,ActionElapsedStore& elapsed,const CargoHandlingWriteContext& c,const SourceRef& actual_cause,const WorldMoment& at,ExactPhaseWriteContext& context){return guard<std::vector<ExactEventApplied>>([&]{if(!source_valid(actual_cause)||!same_moment(take(c.business.read_actual_handling_cause_moment(actual_cause)),at,c.budget))fail("CargoHandlingActualImmediateCauseRequired",actual_cause.id);auto read=read_context(c);std::vector<StableId> ids;for(const auto& row:state.executions.rows())if(!terminal(row.second))ids.push_back(row.first);std::vector<ExactEventApplied> out;
for(const auto& id:ids){auto* e=state.executions.find(id);const auto* t=e?state.terms.find(e->terms_ref):nullptr;if(!e||!t)fail("CargoHandlingTermsRequired",id);auto stamp=current_stamp(read,*t,*e);auto ending=take(c.business.read_pending_handling_termination(*t,*e,at,*stamp));if(ending&&same_source(ending->actual_cause,actual_cause)){if(!same_moment(ending->at,at,c.budget))fail("CargoHandlingCauseMomentMismatch",id);auto result=take(terminate_cargo_handling_in_candidate(common,state,elapsed,c,id,ending->status,actual_cause,at));out.push_back({{"travel","cargo_handling_result",result},1});continue;}if(!active(*e))continue;stamp=current_stamp(read,*t,*e);auto change=take(c.labor.read_pending_handling_labor_change(*t,*e,at,*stamp));if(change&&same_source(change->actual_cause,actual_cause)){if(!same_moment(change->at,at,c.budget))fail("CargoHandlingCauseMomentMismatch",id);take(change_cargo_handling_service_in_candidate(common,state,elapsed,c,id,change->resume,actual_cause,at));out.push_back({{"travel","cargo_handling_execution",id},1});}}
return out;});}
Status validate_cargo_handling_applied_source(const CommonStore& common,const CargoHandlingState& state,const ExactEventApplied& applied){return status([&]{const auto& ref=applied.applied_result;const auto* header=common.state().facts.find(ref.id);if(applied.source_schema!=1||ref.domain!="travel"||!header||header->domain!=ref.domain||header->kind!=ref.kind)fail("CargoHandlingAppliedSourceRequired",ref.id);if(ref.kind=="cargo_handling_execution"){if(!state.executions.find(ref.id))fail("CargoHandlingExecutionRequired",ref.id);}else if(ref.kind=="cargo_handling_work_result"){const auto* work=state.work_results.find(ref.id);const auto* e=work?state.executions.find(work->execution_ref):nullptr;if(!e||e->work_result_ref!=std::optional<StableId>{ref.id})fail("CargoHandlingAppliedWorkMismatch",ref.id);}else if(ref.kind=="cargo_handling_result"){const auto* result=state.results.find(ref.id);const auto* e=result?state.executions.find(result->execution_ref):nullptr;if(!e||e->terminal_result_ref!=std::optional<StableId>{ref.id})fail("CargoHandlingAppliedResultMismatch",ref.id);}else fail("CargoHandlingAppliedKindMismatch",ref.id);});}
}
