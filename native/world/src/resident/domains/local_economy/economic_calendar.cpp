#include "economic_calendar.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id,ErrorKind::internal).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{x.error};return std::move(x.value);}
void take(Status x){if(!x.ok)throw Failure{x.error};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
void unique(const std::vector<StableId>& rows){std::set<StableId> ids;for(const auto& id:rows)if(id.empty()||!ids.insert(id).second)fail("DuplicateEconomicOpeningInput",id);}
std::size_t fact_position(const CommonStore& common,const SourceRef& source){std::size_t n=0;for(const auto& row:common.state().facts.rows()){if(row.first==source.id){if(row.second.kind!=source.kind||row.second.domain!=source.domain)fail("EconomicOpeningSourceHeaderMismatch",source.id);return n;}++n;}fail("EconomicOpeningOriginalSourceRequired",source.id);}
ScenarioOriginRef original_origin(const EconomicCalendarIdentityReadContext& x){
 if(x.foundation_catalog.actors.empty())fail("EconomicCalendarScenarioOriginRequired");
 const auto& original=x.foundation_catalog.actors.rows().front().second.scenario_origin_ref;
 if(original.initial_tick<0||original.catalog_digest!=x.foundation_catalog.identity.content_digest||original.scenario_id!=x.foundation_catalog.scenario_id)fail("EconomicCalendarScenarioOriginMismatch");
 for(const auto& row:x.foundation_catalog.actors.rows()){const auto& origin=row.second.scenario_origin_ref;if(origin.initial_tick!=original.initial_tick||origin.catalog_digest!=original.catalog_digest||origin.scenario_id!=original.scenario_id)fail("EconomicCalendarInconsistentScenarioOrigin");}
 bool adopted=false;
 // Construction-receipt keys belong to S4; match the real adopted origin payload.
 for(const auto& row:x.foundation.actor_start_context.construction_receipts.rows())if(actor_foundation_origin_equal(row.second.scenario_origin_ref,original)){if(adopted)fail("DuplicateEconomicCalendarScenarioAdoption");adopted=true;}
 bool retained=false;for(const auto& row:x.foundation.actor_start_context.scenario_origins)if(actor_foundation_origin_equal(row.ref,original))retained=true;
 if(!adopted||!retained)fail("EconomicCalendarScenarioAdoptionRequired",original.row_key);
 return original;
}
const ActionTimeRegistration& action_registration(const EconomicCalendarReadContext& x,const StableId& id){
 const ActionTimeRegistration* found=nullptr;
 struct MatchActualOwner final:ActionTimeOwnerIdSink {std::string_view requested;bool matched=false;explicit MatchActualOwner(std::string_view value):requested(value){}Status append(std::string_view actual)override{if(actual==requested)matched=true;return common_success();}};
 for(const auto& registration:x.action_registrations){MatchActualOwner owners{id};take(x.action_sources.enumerate_action_time_owners(registration,owners));if(owners.matched){if(found)fail("DuplicateEconomicOpeningActionOwner",id);found=&registration;}}
 if(!found)fail("EconomicOpeningActionOwnerRequired",id);return *found;
}
SourceRef moment_source(const ElapsedTimeSource& source){return std::visit([](const auto& value){return value.source_ref;},source);}
void validate_action(const EconomicCalendarReadContext& x,const StableId& id,const EconomicPeriodOpening& opening){
 const auto& registration=action_registration(x,id);auto original=take(x.action_sources.read_action_time_owner(registration,id));take(validate_action_time_owner_proof(registration,original));
 const auto start=take(x.action_sources.read_action_time_source(original.started_source));
 if(compare(start,opening.opening_at)>0)fail("EconomicOpeningIncludesLaterAction",id);
 const auto cut=fact_position(x.common,economic_period_opening_source(opening));
 if(compare(start,opening.opening_at)==0&&fact_position(x.common,moment_source(original.started_source))>=cut)fail("EconomicOpeningIncludesLaterSameMomentAction",id);
 if(original.ended_source){const auto end=take(x.action_sources.read_action_time_source(*original.ended_source));if(compare(end,opening.opening_at)<0)fail("EconomicOpeningIncludesEndedAction",id);if(compare(end,opening.opening_at)==0&&fact_position(x.common,moment_source(*original.ended_source))<cut)fail("EconomicOpeningIncludesEndedSameMomentAction",id);}
}
void validate_opening_shape(const EconomicCalendarIdentityReadContext& x,const EconomicPeriodOpening& p){
 if(p.source_id.empty()||p.period_id.empty()||p.opening_at.half_day_tick<0||compare(p.opening_at,{p.opening_at.half_day_tick,{}})||p.opening_at.half_day_tick==std::numeric_limits<Integer>::max()||p.through_tick!=p.opening_at.half_day_tick+1)fail("InvalidEconomicPeriodOpening",p.source_id);
 fact_position(x.common,economic_period_opening_source(p));unique(p.existing_activity_ids);unique(p.existing_action_ids);
}
void validate_opening(const EconomicCalendarReadContext& x,const EconomicPeriodOpening& p){
 validate_opening_shape(economic_calendar_identity_context(x),p);
 for(const auto& id:p.existing_activity_ids)take(x.organization_history.read_organization_activity_at_opening(id,economic_period_opening_source(p),p.opening_at));
 for(const auto& id:p.existing_action_ids)validate_action(x,id,p);
}
const EconomicPeriodOpening& opening_for_period(const EconomicCalendarReadContext& x,const ExactPeriodContext& period){
 const auto* value=x.sources.period_openings.find(period.period_id);
 if(!value||value->period_id!=period.period_id||compare(value->opening_at,period.opening_at)||value->through_tick!=period.through_tick)fail("EconomicPeriodOpeningRequired",period.period_id);
 validate_opening(x,*value);return *value;
}
void validate_calendar_identity(const EconomicCalendarIdentityReadContext& x,const WorldMoment& now){
 if(compare(take(x.clock.read_current_moment()),now)||!x.sources.calendar_origin||!actor_foundation_origin_equal(*x.sources.calendar_origin,original_origin(x)))fail("EconomicCalendarOriginOrClockMismatch");
 Integer next=x.sources.calendar_origin->initial_tick;std::set<StableId> source_ids;bool open=false;std::size_t prior_position=0;bool has_prior=false;
 for(const auto& row:x.sources.period_openings.rows()){const auto& p=row.second;if(row.first!=p.period_id||!source_ids.insert(p.source_id).second||p.opening_at.half_day_tick!=next||open)fail("EconomicCalendarOpeningHistoryGap",row.first);validate_opening_shape(x,p);const auto position=fact_position(x.common,economic_period_opening_source(p));if(has_prior&&position<=prior_position)fail("EconomicCalendarOpeningOrderMismatch",row.first);prior_position=position;has_prior=true;if(compare(p.opening_at,now)>0)fail("FutureEconomicCalendarOpening",row.first);if(p.through_tick>x.economy.last_step_tick){if(p.opening_at.half_day_tick!=x.economy.last_step_tick||compare(now,{p.through_tick,{}})>0)fail("EconomicCalendarUnclosedStep",row.first);open=true;}next=p.through_tick;}
 const Integer closed_through=open?next-1:next;
 if(x.economy.last_step_tick!=closed_through||x.economy.last_step_tick>now.half_day_tick)fail("EconomicCalendarCursorCoverageMismatch");
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="local_economy"&&row.second.kind=="economic_period_opened"&&!source_ids.count(row.first))fail("EconomicCalendarOpeningPayloadMissing",row.first);
}
ExactPeriodOwnerSources owner_sources(const EconomicPeriodOpening& p){return {economic_period_opening_source(p),1,{},{}};}
}
EconomicCalendarIdentityReadContext economic_calendar_identity_context(const EconomicCalendarReadContext& x)noexcept{
 return {x.common,x.economy,x.sources,x.foundation,x.foundation_catalog,x.clock};
}
SourceRef economic_period_opening_source(const EconomicPeriodOpening& p){return {"local_economy","economic_period_opened",p.source_id};}
Status initialize_economic_calendar_origin_in_candidate(EconomicWorkSourcesState& state,const EconomicCalendarReadContext& x){try{
 if(&state!=&x.sources)fail("EconomicCalendarCandidateMismatch");const auto origin=original_origin(economic_calendar_identity_context(x));const auto now=take(x.clock.read_current_moment());
 if(compare(now,{origin.initial_tick,{}})||x.economy.last_step_tick!=origin.initial_tick||!state.period_openings.empty()||!x.economy.payments.empty()||!x.economy.rations.empty()||!x.economy.production_receipts.empty()||!x.economy.arrears.empty())fail("EconomicCalendarActualInitializationRequired");
 if(state.calendar_origin&&!actor_foundation_origin_equal(*state.calendar_origin,origin))fail("EconomicCalendarOriginAlreadyBound");state.calendar_origin=origin;return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<const EconomicPeriodOpening*> read_economic_period_opening_identity(const EconomicCalendarIdentityReadContext& x,const SourceRef& ref,Integer schema){try{
 if(schema!=1||ref.domain!="local_economy"||ref.kind!="economic_period_opened")fail("UnsupportedEconomicPeriodOpeningSource",ref.id);
 validate_calendar_identity(x,take(x.clock.read_current_moment()));const EconomicPeriodOpening* found=nullptr;
 for(const auto&row:x.sources.period_openings.rows())if(row.second.source_id==ref.id){if(found||row.first!=row.second.period_id)fail("DuplicateEconomicPeriodOpeningSource",ref.id);found=&row.second;}
 if(!found)fail("EconomicPeriodOpeningSourceMissing",ref.id);return {true,found,{}};
 }catch(const Failure&f){return {false,{},f.error};}}
Outcome<const EconomicPeriodOpening*> read_economic_period_opening_identity(const EconomicCalendarReadContext& x,const SourceRef& ref,Integer schema){
 return read_economic_period_opening_identity(economic_calendar_identity_context(x),ref,schema);
}
Outcome<const EconomicPeriodOpening*> read_economic_period_opening(const EconomicCalendarReadContext& x,const SourceRef& ref,Integer schema){try{
 const auto*found=take(read_economic_period_opening_identity(x,ref,schema));validate_opening(x,*found);return {true,found,{}};
 }catch(const Failure&f){return {false,{},f.error};}}
Outcome<ExactPeriodOwnerSources> open_economic_calendar_period_in_candidate(CommonStore& common,EconomicWorkSourcesState& state,const EconomicCalendarReadContext& x,const ExactPeriodContext& period){try{
 if(&common!=&x.common||&state!=&x.sources)fail("EconomicCalendarCandidateMismatch");if(!state.calendar_origin||!actor_foundation_origin_equal(*state.calendar_origin,original_origin(economic_calendar_identity_context(x))))fail("EconomicCalendarOriginRequired");
 const auto now=take(x.clock.read_current_moment());if(compare(now,period.opening_at)||compare(now,{now.half_day_tick,{}})||now.half_day_tick==std::numeric_limits<Integer>::max()||period.through_tick!=now.half_day_tick+1||period.period_id.empty())fail("EconomicCalendarActualOpeningRequired",period.period_id);
 if(state.period_openings.find(period.period_id))return {true,owner_sources(opening_for_period(x,period)),{}};
 if(x.economy.last_step_tick!=period.opening_at.half_day_tick)fail("EconomicCalendarPriorStepRequired",period.period_id);
 for(const auto& row:state.period_openings.rows())if(row.second.opening_at.half_day_tick>=period.opening_at.half_day_tick)fail("DuplicateEconomicCalendarOpening",period.period_id);
 EconomicPeriodOpening p;p.period_id=period.period_id;p.opening_at=period.opening_at;p.through_tick=period.through_tick;
 p.existing_action_ids=common.action_order();
 for(const auto& row:x.organization.activities.rows())if(row.second.status==OrgActivityStatus::approved||row.second.status==OrgActivityStatus::active||row.second.status==OrgActivityStatus::waiting)p.existing_activity_ids.push_back(row.first);
 p.source_id=take(common.next_id("economic_period_opening"));take(common.append_fact({p.source_id,"economic_period_opened","local_economy"}));
 // The caller owns the whole opening candidate; a later failed provider rolls back this
 // actual header, its ID, and this payload together, just like the original business step.
 if(!state.period_openings.insert(p.period_id,p))fail("DuplicateEconomicCalendarOpening",p.period_id);validate_opening(x,p);return {true,owner_sources(p),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<ExactPeriodOwnerSources> open_economic_production_calendar_period_in_candidate(CommonStore&common,const LocalEconomyCatalog&catalog,EconomicProductionContext&production,const EconomicCalendarReadContext&x,const ExactPeriodContext&period){try{
 if(&common!=&x.common||&production.state!=&x.sources||&production.clock!=&x.clock||compare(take(x.clock.read_current_moment()),period.opening_at))fail("EconomicProductionOpeningSnapshotMismatch",period.period_id);
 if(x.sources.period_openings.find(period.period_id))return open_economic_calendar_period_in_candidate(common,production.state,x,period);
 struct Pending{StableId unit_id;PreparedProductionInputClose close;};std::vector<Pending>pending;
 for(const auto&row:production.state.active_cycles_by_unit.rows()){
  const auto*cycle=production.state.cycles.find(row.second);if(!cycle||cycle->unit_id!=row.first)fail("EconomicProductionOpeningCycleMissing",row.first);if(cycle->settlement_receipt_ref)continue;
  const auto boundary=take(production.inputs.read_next_production_rule_boundary(row.first,row.second));if(!boundary)continue;
  if(compare(*boundary,period.opening_at)<0)fail("EconomicProductionMissedRuleBoundary",row.first);if(compare(*boundary,period.opening_at))continue;
  if(compare(period.opening_at,{cycle->due_tick,{}})>=0)fail("EconomicProductionDueBeforeOpeningRequired",row.first);
  pending.push_back({row.first,take(prepare_production_input_close(common,x.economy,catalog,production,row.first,period.opening_at))});
 }
 auto result=take(open_economic_calendar_period_in_candidate(common,production.state,x,period));const auto&opening=opening_for_period(x,period);const MomentOwnerSource source{economic_period_opening_source(opening),1};
 for(auto&entry:pending){take(apply_prepared_production_input_close_in_candidate(common,production,std::move(entry.close),source.source_ref));take(install_changed_production_input_source_in_candidate(production.state,production.inputs,entry.unit_id,period.opening_at,source));}
 return {true,std::move(result),{}};
 }catch(const Failure&f){return {false,{},f.error};}}
Outcome<DynamicStepInput> read_economic_calendar_input(const EconomicCalendarReadContext& x,const ExactPeriodContext& period){try{const auto& p=opening_for_period(x,period);return {true,{p.opening_at.half_day_tick,p.through_tick,p.existing_activity_ids,{},p.existing_action_ids},{}};}catch(const Failure& f){return {false,{},f.error};}}
Outcome<ExactPeriodOwnerSources> close_economic_calendar_period(const EconomicCalendarReadContext& x,const ExactPeriodContext& period){try{const auto& p=opening_for_period(x,period);if(compare(take(x.clock.read_current_moment()),{p.through_tick,{}})||x.economy.last_step_tick!=p.through_tick)fail("EconomicCalendarOriginalStepNotClosed",p.period_id);return {true,owner_sources(p),{}};}catch(const Failure& f){return {false,{},f.error};}}
Outcome<ExactPeriodOwnerSources> read_economic_calendar_period_sources(const EconomicCalendarReadContext& x,const ExactPeriodContext& period){try{return {true,owner_sources(opening_for_period(x,period)),{}};}catch(const Failure& f){return {false,{},f.error};}}
Status validate_economic_calendar_sources(const EconomicCalendarReadContext& x,const WorldMoment& now){try{
 validate_calendar_identity(economic_calendar_identity_context(x),now);for(const auto&row:x.sources.period_openings.rows())validate_opening(x,row.second);return common_success();
 }catch(const Failure&f){return {false,{},f.error};}}
Outcome<EconomicCalendarOpeningDependencies> read_economic_calendar_opening_dependencies(const EconomicCalendarReadContext& x,const SourceRef& source){try{const auto* p=take(read_economic_period_opening(x,source,1));if(!x.sources.calendar_origin)fail("EconomicCalendarOriginRequired");EconomicCalendarOpeningDependencies out{*x.sources.calendar_origin,{source},p->existing_activity_ids,p->existing_action_ids};for(const auto& id:p->existing_action_ids){const auto& registration=action_registration(x,id);auto proof=take(x.action_sources.read_action_time_owner(registration,id));out.original_source_refs.push_back(proof.original_execution_source);out.original_source_refs.push_back(moment_source(proof.started_source));if(proof.ended_source)out.original_source_refs.push_back(moment_source(*proof.ended_source));}return {true,std::move(out),{}};}catch(const Failure& f){return {false,{},f.error};}}
}
