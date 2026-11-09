#include "dynamic_objective.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {using namespace movement_detail;namespace {
using Key=std::tuple<StableId,StableId,StableId>;
Key key(const SourceRef& r){return {r.domain,r.kind,r.id};}
SourceRef result_ref(const StableId& id){return {"military","military_objective_result",id};}
std::size_t position(const CommonStore& common,const SourceRef& ref){
 return take(read_actual_common_fact_position(common,ref));
}
void canonicalize(std::vector<SourceRef>& refs){
 for(const auto& r:refs)if(!source_valid(r))fail("DynamicMilitaryObjectiveOriginalSourceRequired",r.id);
 std::sort(refs.begin(),refs.end(),[](const auto& a,const auto& z){return key(a)<key(z);});
 refs.erase(std::unique(refs.begin(),refs.end(),[](const auto& a,const auto& z){return same_source(a,z);}),refs.end());
}
bool same_refs(const std::vector<SourceRef>& a,const std::vector<SourceRef>& z){
 if(a.size()!=z.size())return false;for(std::size_t n=0;n<a.size();++n)if(!same_source(a[n],z[n]))return false;return true;
}
std::vector<SourceRef> judgment_sources(const DynamicObjectiveOriginalInputs& in){
 std::vector<SourceRef> out;
 for(const auto& u:in.actual_units){out.push_back(u.actual_unit.position_source);
  out.insert(out.end(),u.actual_unit.actual_qualification_sources.begin(),u.actual_unit.actual_qualification_sources.end());
  if(u.actual_routing_source)out.push_back(*u.actual_routing_source);
 }
 canonicalize(out);if(out.empty())fail("DynamicMilitaryObjectiveJudgmentSourcesRequired",in.session_ref);return out;
}
std::vector<SourceRef> exit_sources(const DynamicObjectiveOriginalInputs& in){
 std::vector<SourceRef> out;for(const auto& e:in.current.escaped_units)out.push_back(e.actual_exit_source_ref);
 canonicalize(out);return out;
}
const DynamicSessionStateSource2& session_source(const CommonStore& common,const MilitaryExactState& state,
 const SourceRef& ref,const DynamicMilitaryObjectiveEvidence2& result,std::size_t cut,ExactArithmeticBudget b){
 if(ref.domain!="military"||ref.kind!="dynamic_session_state"||position(common,ref)>=cut)
  fail("DynamicMilitaryObjectivePriorSourceRequired",ref.id);
 const auto* row=state.session_state_sources.find(ref.id);
 if(!row||row->id!=ref.id||row->session_ref!=result.session_ref||row->generation!=result.generation||
    take(compare_world_moments(row->effective_at,result.effective_at,b))>0)
  fail("DynamicMilitaryObjectiveSessionSourceMismatch",ref.id);
 return *row;
}
void verify(const CommonStore& common,const MilitaryExactState& state,
 const DynamicMilitarySessionOriginalRead& sessions,const DynamicMilitaryObjectiveOriginalRead& original,
 const CurrentMomentRead& clock,const DynamicMilitaryObjectiveEvidence2& row,ExactArithmeticBudget b){
 if(row.id.empty()||row.session_ref.empty()||row.generation<1||row.objective_definition_ref.empty()||
    row.cause_kind<DynamicObjectiveCause::actual_entry||row.cause_kind>DynamicObjectiveCause::objective_gate||
    row.cause_source_refs.empty()||row.outcome.winner_side< -1||row.outcome.winner_side>1||
    row.outcome.reason.empty()||!row.outcome.field_order.empty())fail("DynamicMilitaryObjectiveResultRequired",row.id);
 if(take(compare_world_moments(row.effective_at,take(clock.read_current_moment()),b))>0)
  fail("DynamicMilitaryObjectiveResultInFuture",row.id);
 const auto cut=position(common,result_ref(row.id));
 const auto& initial=session_source(common,state,row.session_initialization_source_ref,row,cut,b);
 const auto* initialized=std::get_if<DynamicSessionInitialized>(&initial.payload);
 if(!initialized||initialized->objective_definition_ref!=row.objective_definition_ref)
  fail("DynamicMilitaryObjectiveInitializationRequired",row.id);
 take(sessions.validate_actual_session_initialization(initial));
 const auto& transition=session_source(common,state,row.objective_transition_source_ref,row,cut,b);
 if(!same_source(row.objective_transition_source_ref,row.session_initialization_source_ref)){
  const auto* q=std::get_if<DynamicObjectiveQualificationTransition>(&transition.payload);
  if(!q||q->objective_definition_ref!=row.objective_definition_ref)
   fail("DynamicMilitaryObjectiveQualificationSourceRequired",row.id);
  take(sessions.validate_actual_session_transition(transition));
 }
 const auto* current=find_dynamic_military_session_current_row(state,row.session_ref,row.generation);
 if(!current||!same_source(current->initialization_source_ref,row.session_initialization_source_ref)||
    current->definition_lock_ref!=initialized->definition_lock_ref)
  fail("DynamicMilitaryObjectiveSessionRequired",row.id);
 const auto in=take(original.read_original_objective_inputs_before_source(row));
 const auto definition=take(sessions.read_session_definition(initialized->definition_lock_ref));
 if(in.session_ref!=row.session_ref||in.generation!=row.generation||!same_moment(in.effective_at,row.effective_at,b)||
    in.original_definition.id!=row.objective_definition_ref||in.original_timing.definition_lock_ref!=initialized->definition_lock_ref||
    in.original_timing.objective_definition_ref!=definition.objective_definition_ref||
    compare(in.original_timing.deadline_duration.value,definition.deadline_duration.value,b)||
    bool(in.original_timing.hold_duration)!=bool(definition.hold_duration)||
    (definition.hold_duration&&compare(in.original_timing.hold_duration->value,definition.hold_duration->value,b))||
    !same_moment(in.current.started_at,initial.effective_at,b)||
    !same_source(in.current.last_transition_source_ref,row.objective_transition_source_ref)||
    in.actual_cause!=row.cause_kind||!same_refs(in.actual_cause_sources,row.cause_source_refs)||
    !same_refs(judgment_sources(in),row.actual_judgment_source_refs)||!same_refs(exit_sources(in),row.actual_exit_source_refs))
  fail("DynamicMilitaryObjectiveOriginalJudgmentMismatch",row.id);
 if(const auto* held=std::get_if<DynamicHolding>(&in.current.hold_state)){
  const auto* q=std::get_if<DynamicObjectiveQualificationTransition>(&transition.payload);
  if(!q||q->to_state!=DynamicObjectiveQualification::holding||
     !same_source(held->qualification_transition_ref,row.objective_transition_source_ref)||
     !same_moment(held->held_since,transition.effective_at,b))fail("DynamicMilitaryObjectiveHoldingSourceMismatch",row.id);
 }
 if(row.cause_kind==DynamicObjectiveCause::objective_gate){
  auto due=in.current.deadline_at;auto basis=row.session_initialization_source_ref;
  if(const auto* held=std::get_if<DynamicHolding>(&in.current.hold_state)){
   if(!definition.hold_duration)fail("DynamicMilitaryHoldDurationRequired",row.id);
   const auto h=take(add_world_duration(held->held_since,*definition.hold_duration,b));
   if(take(compare_world_moments(h,due,b))<0){due=h;basis=held->qualification_transition_ref;}
  }
  if(!same_moment(due,row.effective_at,b)||row.cause_source_refs.size()!=1||!same_source(row.cause_source_refs.front(),basis))
   fail("DynamicMilitaryObjectiveActualGateRequired",row.id);
 }
 const auto evaluated=take(evaluate_dynamic_military_objective(in,b));
 if(evaluated.qualification_transition||!evaluated.outcome||evaluated.outcome->winner_side!=row.outcome.winner_side||
    evaluated.outcome->reason!=row.outcome.reason)fail("DynamicMilitaryObjectiveOutcomeMismatch",row.id);
}
}
Outcome<const DynamicMilitaryObjectiveEvidence2*> read_verified_dynamic_military_objective_result(
 const CommonStore& common,const MilitaryExactState& state,const DynamicMilitarySessionOriginalRead& sessions,
 const DynamicMilitaryObjectiveOriginalRead& originals,const CurrentMomentRead& clock,
 const MomentOwnerSource& source,ExactArithmeticBudget b){return guard<const DynamicMilitaryObjectiveEvidence2*>([&]{
 if(source.source_schema!=2||source.source_ref.domain!="military"||source.source_ref.kind!="military_objective_result")
  fail("DynamicMilitaryObjectiveResult2Required",source.source_ref.id);
 const auto* row=state.objective_evidence.find(source.source_ref.id);
 if(!row||row->id!=source.source_ref.id)fail("DynamicMilitaryObjectiveResultRequired",source.source_ref.id);
 verify(common,state,sessions,originals,clock,*row,b);return row;
});}
Outcome<std::optional<MomentOwnerSource>> apply_dynamic_military_objective_result_in_candidate(
 CommonStore& common,const MilitaryState& military,MilitaryExactState& state,
 const DynamicMilitarySessionOriginalRead& sessions,const DynamicMilitaryObjectiveOriginalRead& originals,
 const CurrentMomentRead& clock,const StableId& session,Integer generation,DynamicObjectiveCause cause,
 const std::vector<SourceRef>& causes,ExactArithmeticBudget b){return guard<std::optional<MomentOwnerSource>>([&]()->std::optional<MomentOwnerSource>{
 const DynamicMilitaryObjectiveEvidence2* old=nullptr;
 for(const auto& row:state.objective_evidence.rows())if(row.second.session_ref==session&&row.second.generation==generation){
  if(old||row.first!=row.second.id)fail("DuplicateDynamicMilitaryObjectiveResult",session);old=&row.second;
 }
 if(old){if(old->cause_kind!=cause||!same_refs(old->cause_source_refs,causes))fail("DynamicMilitaryObjectiveResultReplayMismatch",old->id);
  verify(common,state,sessions,originals,clock,*old,b);return MomentOwnerSource{result_ref(old->id),2};}
 const auto applied=take(apply_dynamic_military_objective_qualification_in_candidate(common,military,state,
  sessions,originals,clock,session,generation,cause,causes,b));
 if(!applied.evaluation.outcome)return std::nullopt;
 const auto* current=state.session_current_states.find(dynamic_military_session_key(session,generation));
 if(!current||!std::holds_alternative<DynamicMilitaryActive>(current->lifecycle)||current->objective_current.outcome_source_ref)
  fail("DynamicMilitaryActiveObjectiveRequired",session);
 const auto at=take(clock.read_current_moment());
 const auto in=take(originals.read_actual_objective_inputs(*current,at,cause,causes));
 const auto evaluation=take(evaluate_dynamic_military_objective(in,b));
 if(evaluation.qualification_transition||!evaluation.outcome||in.session_ref!=session||in.generation!=generation||
    !same_moment(in.effective_at,at,b)||in.actual_cause!=cause||!same_refs(in.actual_cause_sources,causes)||
    !same_source(in.current.last_transition_source_ref,current->objective_current.last_transition_source_ref)||
    evaluation.outcome->winner_side!=applied.evaluation.outcome->winner_side||
    evaluation.outcome->reason!=applied.evaluation.outcome->reason)fail("DynamicMilitaryObjectiveAfterQualificationMismatch",session);
 DynamicMilitaryObjectiveEvidence2 row;row.id=take(common.next_id("military_objective_result"));row.session_ref=session;
 row.generation=generation;row.effective_at=at;row.objective_definition_ref=current->objective_current.objective_definition_ref;
 row.session_initialization_source_ref=current->initialization_source_ref;
 row.objective_transition_source_ref=current->objective_current.last_transition_source_ref;row.cause_kind=cause;
 row.cause_source_refs=causes;row.actual_judgment_source_refs=judgment_sources(in);row.actual_exit_source_refs=exit_sources(in);
 row.outcome=*evaluation.outcome;row.outcome.field_order.clear();const auto ref=result_ref(row.id);
 take(common.append_fact({ref.id,ref.kind,ref.domain}));
 // The actual append is the same-M historical cut. A caller cannot choose an
 // old ordinal or claim that a payload ID by itself is an observation boundary.
 verify(common,state,sessions,originals,clock,row,b);
 if(!state.objective_evidence.insert(ref.id,std::move(row)))fail("DuplicateDynamicMilitaryObjectiveResult",ref.id);
 return MomentOwnerSource{ref,2};
});}
Status validate_dynamic_military_objective_results(const CommonStore& common,const MilitaryExactState& state,
 const DynamicMilitarySessionOriginalRead& sessions,const DynamicMilitaryObjectiveOriginalRead& originals,
 const CurrentMomentRead& clock,ExactArithmeticBudget b){return status([&]{
 std::set<std::pair<StableId,Integer>> owners;
 for(const auto& entry:state.objective_evidence.rows()){
  const auto& row=entry.second;if(entry.first!=row.id||!owners.emplace(row.session_ref,row.generation).second)
   fail("DuplicateDynamicMilitaryObjectiveResult",entry.first);
  verify(common,state,sessions,originals,clock,row,b);
  const auto* current=find_dynamic_military_session_current_row(state,row.session_ref,row.generation);
  const auto* terminal=current?std::get_if<DynamicMilitaryTerminal>(&current->lifecycle):nullptr;
  const auto* close=terminal?take(read_dynamic_military_terminal_source(common,state,row.session_ref,row.generation,b)):nullptr;
  const auto* payload=close?std::get_if<DynamicSessionTerminated>(&close->payload):nullptr;
  if(!terminal||!payload||!same_source(terminal->outcome_source_ref,result_ref(row.id))||
     !current->objective_current.outcome_source_ref||!same_source(*current->objective_current.outcome_source_ref,result_ref(row.id))||
     !same_source(payload->objective_outcome_source_ref,result_ref(row.id))||!same_moment(terminal->closed_at,row.effective_at,b)||
     !same_moment(close->effective_at,row.effective_at,b)||close->session_ref!=row.session_ref||close->generation!=row.generation||
     position(common,{"military","dynamic_session_state",close->id})<=position(common,result_ref(row.id)))
   fail("DynamicMilitaryObjectiveTerminalConsumerRequired",row.id);
 }
 for(const auto& row:common.state().facts.rows())if(row.second.domain=="military"&&row.second.kind=="military_objective_result"&&
    !state.objective_evidence.find(row.first))fail("DynamicMilitaryObjectiveResultPayloadRequired",row.first);
 for(const auto& row:state.session_current_states.rows())if(const auto* terminal=std::get_if<DynamicMilitaryTerminal>(&row.second.lifecycle)){
  const auto* result=state.objective_evidence.find(terminal->outcome_source_ref.id);
  if(terminal->outcome_source_ref.domain!="military"||terminal->outcome_source_ref.kind!="military_objective_result"||
     !result||result->session_ref!=row.second.session_ref||result->generation!=row.second.generation)
   fail("DynamicMilitaryTerminalObjectiveResultRequired",row.first);
 }
});}
}
