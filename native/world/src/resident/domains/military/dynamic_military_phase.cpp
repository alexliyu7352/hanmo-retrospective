#include "dynamic_military_phase.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <iterator>
#include <set>
#include <tuple>
namespace hanmo::resident {using namespace movement_detail;namespace {
using SessionKey=std::pair<StableId,Integer>;
void current(const DynamicMilitaryActionContext& x,const WorldMoment& at){
 if(!same_moment(take(x.clock.read_current_moment()),at,x.budget))fail("DynamicMilitaryActualPhaseRequired");
}
void read_binding(const DynamicMilitaryVerifiedSourceContext& x){
 if(!x.objectives||&x.actions.clock!=&x.land.clock)fail("DynamicMilitaryPhaseOriginalSourcesRequired");
}
std::vector<SessionKey> session_keys(const MilitaryExactState& state){
 std::vector<SessionKey> out;std::set<SessionKey> unique;
 for(const auto& row:state.session_current_states.rows()){
  const auto& s=row.second;SessionKey key{s.session_ref,s.generation};
  if(s.session_ref.empty()||s.generation<1||row.first!=dynamic_military_session_key(s.session_ref,s.generation)||!unique.insert(key).second)
   fail("DynamicMilitarySessionKeyMismatch",row.first);
  out.push_back(std::move(key));
 }
 // Ordering serializes distinct original sessions, never selects a winning
 // participant within a simultaneous effect or topology group.
 std::sort(out.begin(),out.end());return out;
}
struct Due {WorldMoment at;SourceRef basis;Integer priority=0;};
std::optional<Due> earliest(const CommonStore& common,const MilitaryState& legacy,
 const MilitaryExactState& exact,const DynamicMilitaryVerifiedSourceContext& x,
 const WorldMoment& now,const WorldMoment& upper){
 read_binding(x);current(x.actions,now);const auto b=x.actions.budget;
 if(take(compare_world_moments(now,upper,b))>0)fail("DynamicMilitaryInvalidPredictionWindow");
 std::optional<Due> first;
 auto add=[&](const WorldMoment& at,const SourceRef& basis,Integer priority){
  if(!source_valid(basis)||take(read_dynamic_military_source_schema(basis))!=2)
   fail("DynamicMilitaryPredictionOriginalSourceRequired",basis.id);
  if(take(compare_world_moments(at,now,b))<0)fail("DynamicMilitaryUnconsumedActualGate",basis.id);
  if(take(compare_world_moments(at,upper,b))>0)return;
  if(!first){first=Due{at,basis,priority};return;}
  const auto order=take(compare_world_moments(at,first->at,b));
  if(order<0||(order==0&&std::tie(priority,basis.domain,basis.kind,basis.id)<
   std::tie(first->priority,first->basis.domain,first->basis.kind,first->basis.id)))first=Due{at,basis,priority};
 };
 const auto expiry=take(predict_dynamic_session_expiry(common,exact,x.actions.sessions,now,b));
 if(expiry)for(const auto& e:take(collect_dynamic_session_expiries_at(common,exact,x.actions.sessions,now,*expiry,b)))
  add(e.at,e.actual_activation_source,0);
 const auto sessions=session_keys(exact);
 for(const auto& s:sessions)take(collect_dynamic_military_actions_at(common,exact,x.actions,s.first,s.second,now));
 for(const auto& row:exact.actions.rows()){
  const auto& a=row.second;if(row.first!=a.id||!find_dynamic_military_session_current_row(exact,a.session_ref,a.generation))
   fail("DynamicMilitaryActionSessionMismatch",row.first);
  const SourceRef source{"military","dynamic_tactical_action",a.id};
  const auto accepted=take(read_dynamic_military_action_source_moment(common,exact,source,2));
  if(!same_moment(accepted,a.accepted_at,b))fail("DynamicMilitaryActionSourceMismatch",a.id);
  if(auto due=take(predict_dynamic_military_action_due(a,x.actions)))add(*due,source,1);
 }
 // At the actual expiry gate, stable current intentionally rejects an unclosed
 // status. The expiry's genuine event is enough to enter this same-M closure;
 // objective prediction is repeated only after all expiries are actually closed.
 const bool expiry_now=expiry&&same_moment(*expiry,now,b);
 if(!expiry_now)for(const auto& s:sessions){
  auto gate=take(predict_dynamic_military_objective_gate(common,legacy,exact,x.actions.sessions,s.first,s.second,now,b));
  if(gate)add(gate->at,gate->actual_basis,2);
 }
 return first;
}
DynamicMilitaryVerifiedSourceContext read_context(const DynamicMilitaryGroupWriteContext& x){
 return {x.actions,x.battle_catalog,x.originals,x.land,&x.objectives};
}
void append(std::vector<MomentOwnerSource>& rows,const MomentOwnerSource& source){
 if(!source_valid(source.source_ref)||source.source_schema!=2)fail("DynamicMilitaryActualResultRequired",source.source_ref.id);
 if(std::any_of(rows.begin(),rows.end(),[&](const auto& v){return same_source(v.source_ref,source.source_ref);}))
  fail("DuplicateDynamicMilitaryAppliedSource",source.source_ref.id);
 rows.push_back(source);
}
// 按原顺序以移动构造追加，不要求元素支持移动赋值。
template <class T>
void append_moves(std::vector<T> &out, std::vector<T> &values) {
  for (auto &value : values) {
    out.emplace_back(std::move(value));
  }
}
}
Outcome<std::vector<ExactEventPrediction>> predict_exact_dynamic_military_events(
 const CommonStore& c,const MilitaryState& m,const MilitaryExactState& s,
 const DynamicMilitaryVerifiedSourceContext& x,const WorldMoment& now,const WorldMoment& upper){
 return guard<std::vector<ExactEventPrediction>>([&]{std::vector<ExactEventPrediction> out;
  if(auto due=earliest(c,m,s,x,now,upper))out.push_back({due->at,ExactStage::same_moment_hits,due->basis,2,0});return out;
 });
}
Status provide_exact_dynamic_military_tactical_interval(const CommonStore& common,
 MilitaryExactState& exact,const DynamicMilitaryActionContext& x,const MomentSpan& span){return status([&]{
 current(x,span.to);if(take(compare_world_moments(span.from,span.to,x.budget))>=0)fail("DynamicMilitaryPositiveActualSpanRequired");
 for(const auto& row:exact.actions.rows()){
  const auto& a=row.second;if(row.first!=a.id)fail("DynamicMilitaryActionIdentityMismatch",row.first);
  if(!std::holds_alternative<DynamicTacticalPreparing>(a.phase)&&!std::holds_alternative<DynamicTacticalRecovering>(a.phase))continue;
  const auto* session=exact.session_current_states.find(dynamic_military_session_key(a.session_ref,a.generation));
  if(!session||!std::holds_alternative<DynamicMilitaryActive>(session->lifecycle))fail("DynamicMilitaryActiveSessionRequired",a.id);
  take(provide_dynamic_military_action_service_in_candidate(common,exact,x,a.id,span));
 }
});}
Status validate_exact_dynamic_military_phase_sources(const CommonStore& common,
 const MilitaryState& legacy,const MilitaryExactState& exact,
 const DynamicMilitaryVerifiedSourceContext& x,const WorldMoment& at){return status([&]{
 read_binding(x);current(x.actions,at);take(validate_dynamic_military_sessions(common,legacy,exact,x.actions.sessions,at,x.actions.budget));
 take(validate_dynamic_military_actions(common,legacy,exact,x.actions,at));
 take(validate_dynamic_military_effect_action_coverage(common,exact,x.actions.catalog,x.actions.budget));
 take(validate_dynamic_military_effect_groups(common,legacy,exact,x.battle_catalog,x.actions.catalog,x.effects,x.actions.clock,x.actions.budget));
 for(const auto& row:exact.dynamic_casualty_facts.rows())take(validate_dynamic_military_land_consequence(common,legacy,exact,x.land,{"military","military_casualties",row.first}));
 take(validate_dynamic_military_objective_results(common,exact,x.actions.sessions,*x.objectives,x.actions.clock,x.actions.budget));
 if(earliest(common,legacy,exact,x,at,at))fail("DynamicMilitaryMomentClosureIncomplete");
});}
Outcome<ExactEventApplied> apply_exact_dynamic_military_event(CommonStore& common,
 MilitaryState& legacy,MilitaryExactState& exact,const DynamicMilitaryGroupWriteContext& x,
 const ExactEventPrediction& prediction,ExactPhaseWriteContext& writes){return guard<ExactEventApplied>([&]{
 const auto read=read_context(x);const auto b=x.actions.budget;current(x.actions,prediction.at);
 if(prediction.stage!=ExactStage::same_moment_hits||prediction.source_schema!=2||prediction.dependency_revision)
  fail("DynamicMilitaryPredictionSourceMismatch",prediction.actual_source_basis.id);
 const auto actual=earliest(common,legacy,exact,read,prediction.at,prediction.at);
 if(!actual||!same_source(actual->basis,prediction.actual_source_basis))fail("StaleDynamicMilitaryPrediction",prediction.actual_source_basis.id);
 std::vector<MomentOwnerSource> applied;
 // These are true original transition rows, including every other session at M.
 for(const auto& source:take(apply_dynamic_session_expiries_in_candidate(common,legacy,exact,x.actions.sessions,x.actions.clock,b)))append(applied,source);
 const auto sessions=session_keys(exact);
 // Close every true recovery before taking any session's effect snapshot.
 // A released projectile is independent and remains in the subsequent due set.
 for(const auto& session:sessions)for(const auto& due:take(collect_dynamic_military_actions_at(common,exact,x.actions,session.first,session.second,prediction.at))){
  if(due.committed_projectile||due.stage!=MilitaryTacticalWorkStage::recovery)continue;
  take(complete_dynamic_military_action_recovery_in_candidate(common,exact,x.actions,due.action_ref));
  const auto* action=exact.actions.find(due.action_ref);
  const auto* completed=action?std::get_if<DynamicTacticalCompleted>(&action->phase):nullptr;
  if(!completed||!same_moment(completed->completed_at,prediction.at,b))fail("DynamicMilitaryActualRecoveryRequired",due.action_ref);
  append(applied,{completed->result_source_ref,2});
 }
 for(const auto& session:sessions){
  if(take(collect_dynamic_military_actions_at(common,exact,x.actions,session.first,session.second,prediction.at)).empty())continue;
  auto result=take(apply_dynamic_land_effect_group_in_candidate(common,legacy,exact,x,session.first,session.second));
  if(result.already_applied)fail("DynamicMilitaryUnconsumedGateReusedGroup",session.first);
  for(const auto& source:result.actual_recovery_sources)append(applied,source);
  for(const auto* source:{&result.effect_group,&result.battle,&result.casualty,&result.objective,&result.terminal})if(*source)append(applied,**source);
  append_moves(writes.condition_writes,result.named.actual_writes);
  append_moves(writes.availability_writes,result.named.availability_writes);
  append_moves(writes.spatial_location_writes,result.actual_spatial_writes);
 }
 // A real Hold/Deadline gate needs no invented attack/effect. Re-read after
 // all same-M effects; a terminated session or invalidated Hold no longer fires.
 for(const auto& session:sessions){
  auto gate=take(predict_dynamic_military_objective_gate(common,legacy,exact,x.actions.sessions,session.first,session.second,prediction.at,b));
  if(!gate||!same_moment(gate->at,prediction.at,b))continue;
  const auto* before=exact.session_current_states.find(dynamic_military_session_key(session.first,session.second));
  if(!before)fail("DynamicMilitarySessionCurrentRequired",session.first);const auto previous=before->last_session_transition_ref;
  auto outcome=take(apply_dynamic_military_objective_result_in_candidate(common,legacy,exact,x.actions.sessions,x.objectives,x.actions.clock,
   session.first,session.second,DynamicObjectiveCause::objective_gate,{gate->actual_basis},b));
  const auto* after=exact.session_current_states.find(dynamic_military_session_key(session.first,session.second));
  if(!after)fail("DynamicMilitarySessionCurrentRequired",session.first);
  if(!same_source(previous,after->last_session_transition_ref))append(applied,{after->last_session_transition_ref,2});
  if(outcome){append(applied,*outcome);append(applied,take(terminate_dynamic_military_session_from_objective_in_candidate(common,legacy,exact,x.actions,x.objectives,*outcome)));}
 }
 if(applied.empty())fail("DynamicMilitaryActualPhaseResultRequired",prediction.actual_source_basis.id);
 take(validate_exact_dynamic_military_phase_sources(common,legacy,exact,read,prediction.at));
 // The enclosing graph accepts one actual source. All other outputs remain in
 // their real original tables and the same live capture, never a batch fact.
 const auto& result=applied.back();return ExactEventApplied{result.source_ref,result.source_schema};
});}
Status validate_exact_dynamic_military_applied_source(const CommonStore& common,
 const MilitaryState& legacy,const MilitaryExactState& exact,
 const DynamicMilitaryVerifiedSourceContext& x,const ExactEventApplied& applied){return status([&]{
 read_binding(x);const auto actual=take(read_verified_dynamic_military_source_moment(common,legacy,exact,x,{applied.applied_result,applied.source_schema}));
 const auto now=take(x.actions.clock.read_current_moment());if(!same_moment(actual,now,x.actions.budget))fail("DynamicMilitaryAppliedMomentMismatch",applied.applied_result.id);
 take(validate_exact_dynamic_military_phase_sources(common,legacy,exact,x,now));
});}
}
