#include "care_period.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "care_period_original_action.hpp"
#include <algorithm>
#include <limits>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same(const SourceRef& a,const SourceRef& b){return std::tie(a.domain,a.kind,a.id)==std::tie(b.domain,b.kind,b.id);}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same(a.source_ref,b.source_ref);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
void text(const StableId& id){if(id.empty()||id.size()>256)fail("InvalidCarePeriodIdentifier",id);}
void source(const MomentOwnerSource& s){text(s.source_ref.domain);text(s.source_ref.kind);text(s.source_ref.id);if(s.source_schema<1)fail("InvalidCarePeriodOriginalSourceSchema",s.source_ref.id);if(s.source_ref.domain=="care_requirements"&&s.source_ref.kind=="care_period")fail("CarePeriodCannotProveItself",s.source_ref.id);}
void refs(const std::vector<MomentOwnerSource>& values){if(values.size()>65536)fail("CarePeriodSourceLimit");for(std::size_t n=0;n<values.size();++n){source(values[n]);for(std::size_t p=0;p<n;++p)if(same(values[n].source_ref,values[p].source_ref))fail("DuplicateCarePeriodOriginalSource",values[n].source_ref.id);}}
const CommonStore& context(const CarePeriodSources& x){const auto& g=x.coverage.handovers.acceptance.goals;if(&x.calendar.common!=&g.common||!x.originals.bound_to(g.common)||(g.rules.selected_care_version()!=5&&g.rules.selected_care_version()!=6))fail("CarePeriodOriginalOwnersRequired",{},ErrorKind::unsupported_capability);if(!same(take(g.clock.read_current_moment()),take(x.calendar.clock.read_current_moment())))fail("CarePeriodActualClockMismatch");return g.common;}
const FactRecord& header(const CommonStore& c,const SourceRef& s){const auto* row=c.state().facts.find(s.id);if(!row||row->id!=s.id||row->domain!=s.domain||row->kind!=s.kind||std::count(c.state().facts.order().begin(),c.state().facts.order().end(),s.id)!=1)fail("CarePeriodActualFactRequired",s.id);return *row;}
void before(const CommonStore& c,const SourceRef& a,const SourceRef& b){header(c,a);header(c,b);const auto& o=c.state().facts.order();if(std::find(o.begin(),o.end(),a.id)>=std::find(o.begin(),o.end(),b.id))fail("CarePeriodOriginalSourceOrderMismatch",b.id);}
const EconomicPeriodOpening& opening(const CarePeriodSources& x,const CarePeriod& row,const CarePeriodSource& source){
 const auto* actual=take(read_economic_period_opening_identity(x.calendar,source.calendar_period_ref.source_ref,source.calendar_period_ref.source_schema));
 if(!actual||actual->opening_at.half_day_tick!=row.period.from_tick||!same(actual->opening_at,WorldMoment{row.period.from_tick,{}})||actual->through_tick!=row.period.to_tick||actual->through_tick!=actual->opening_at.half_day_tick+1)fail("CarePeriodCalendarOpeningMismatch",source.care_period_ref);
 return *actual;
}
bool participation_equal(const CarePeriodParticipation& a,const CarePeriodParticipation& b){if(a.index()!=b.index())return false;if(const auto* x=std::get_if<OriginalCareActionParticipation>(&a)){const auto& y=std::get<OriginalCareActionParticipation>(b);return same(x->original_action_ref,y.original_action_ref)&&same(x->provided_period_ref,y.provided_period_ref);}const auto& x=std::get<CompatibleFamilyActivityParticipation>(a);const auto& y=std::get<CompatibleFamilyActivityParticipation>(b);return same(x.family_activity_ref,y.family_activity_ref)&&actor_foundation_rule_equal(x.compatibility_rule_ref,y.compatibility_rule_ref)&&same(x.actual_participation_ref,y.actual_participation_ref);}
const EconomicPeriodOpening& validate_row(const CarePeriodSources& x,const StableId& id,const CarePeriod& row,const CarePeriodSource& source){
 const auto& c=context(x);if(source.care_period_ref!=id)fail("CarePeriodSourceKeyMismatch",id);check(validate_care_period_shape(row));check(validate_care_period_source_shape(source));const auto ref=care_period_source(id);header(c,ref);
 const auto coverage=take(read_care_binding_coverage_v2(x.coverage,source.binding_ref));if(coverage.binding->child_actor_id!=row.child||coverage.binding->carer_actor_id!=row.caregiver)fail("CarePeriodBindingActorMismatch",id);
 check(validate_care_binding_full_calendar_period(x.coverage,source.binding_ref,row.period.from_tick,row.period.to_tick));const auto& period=opening(x,row,source);
 before(c,coverage.handover_completed->original_transition_ref,ref);before(c,source.calendar_period_ref.source_ref,ref);
 bool food=false;for(const auto& original:source.provision_result_refs)if(same(original,row.food_support_ref))food=true;if(!food)fail("CarePeriodOriginalFoodResultMissing",id);
 if(const auto* action=std::get_if<OriginalCareActionParticipation>(&source.participation)){
  if(!same(action->original_action_ref,row.activity_ref)||!same(action->provided_period_ref,source.actual_complete_source_ref))fail("CarePeriodOriginalActionMismatch",id);
  if(!x.original_actions||&x.original_actions->common!=&c)fail("CarePeriodOriginalActionOwnersRequired",id,ErrorKind::unsupported_capability);
  if(!same(take(x.original_actions->clock.read_current_moment()),take(x.coverage.handovers.acceptance.goals.clock.read_current_moment())))fail("CarePeriodOriginalActionClockMismatch",id);
  const auto actual=take(read_care_original_action_period(*x.original_actions,*action));
  if(*actual.actor!=row.caregiver||*actual.child!=row.child||actual.from_tick!=row.period.from_tick||actual.to_tick!=row.period.to_tick)fail("CarePeriodActualOriginalActionMismatch",id);
  // The retained original start must predate this period's Opening. The
  // original action owner separately proves the accepted integer commitment.
  before(c,action->original_action_ref.source_ref,source.calendar_period_ref.source_ref);
  before(c,action->provided_period_ref.source_ref,ref);
  if(std::find(period.existing_action_ids.begin(),period.existing_action_ids.end(),action->original_action_ref.source_ref.id)==period.existing_action_ids.end())fail("CarePeriodActionNotAdoptedAtOpening",id);
 }else if(!same(std::get<CompatibleFamilyActivityParticipation>(source.participation).family_activity_ref,row.activity_ref))fail("CarePeriodCompatibleActivityMismatch",id);
 for(const auto& pair:x.tables.sources.rows())if(pair.first!=id){
  if(pair.second.binding_ref==source.binding_ref&&same(pair.second.calendar_period_ref,source.calendar_period_ref))fail("DuplicateCareBindingCalendarPeriod",id);
  const auto* other=x.tables.periods.find(pair.first);if(other&&other->child==row.child&&other->period.from_tick==row.period.from_tick&&other->period.to_tick==row.period.to_tick)fail("DuplicateCareChildCalendarPeriod",id);
 }
 check(x.originals.validate_original_completed_care_period(*coverage.binding,*coverage.accepted,row,source,period,ref));
 return period;
}
}
SourceRef care_period_source(const StableId& id){return {"care_requirements","care_period",id};}
Outcome<Integer> care_period_source_schema(const SourceRef& s){return s.domain=="care_requirements"&&s.kind=="care_period"&&!s.id.empty()?Outcome<Integer>{true,3,{}}:Outcome<Integer>{false,{},common_failure("UnknownCarePeriodSource",s.id,ErrorKind::unsupported_capability).error};}
Status validate_care_period_shape(const CarePeriod& row){try{text(row.child);text(row.caregiver);source(row.place);source(row.activity_ref);source(row.food_support_ref);if(row.child==row.caregiver||row.period.from_tick<0||row.period.from_tick==std::numeric_limits<Integer>::max()||row.period.to_tick!=row.period.from_tick+1||row.outcome!=CarePeriodOutcome::completed)fail("InvalidCompleteCarePeriod",row.child);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_period_source_shape(const CarePeriodSource& row){try{
 text(row.care_period_ref);text(row.binding_ref);source(row.calendar_period_ref);source(row.actual_complete_source_ref);
 if(row.calendar_period_ref.source_ref.domain!="local_economy"||row.calendar_period_ref.source_ref.kind!="economic_period_opened"||row.calendar_period_ref.source_schema!=1)fail("CarePeriodOriginalCalendarRequired",row.care_period_ref);
 if(const auto* action=std::get_if<OriginalCareActionParticipation>(&row.participation)){
  source(action->original_action_ref);source(action->provided_period_ref);const auto& a=action->original_action_ref.source_ref;const auto& p=action->provided_period_ref.source_ref;
  const bool child=a.domain=="family_birth"&&a.kind=="family_child_care_started"&&p.domain==a.domain&&p.kind=="family_child_care";
  const bool relief=a.domain=="family_care_relief"&&a.kind=="family_care_relief_started"&&p.domain==a.domain&&p.kind=="family_care_relief_provided";
  if((!child&&!relief)||action->original_action_ref.source_schema!=1||action->provided_period_ref.source_schema!=1)fail("CarePeriodRegisteredOriginalActionRequired",row.care_period_ref,ErrorKind::unsupported_capability);
 }else{const auto& activity=std::get<CompatibleFamilyActivityParticipation>(row.participation);source(activity.family_activity_ref);source(activity.actual_participation_ref);text(activity.compatibility_rule_ref.id);text(activity.compatibility_rule_ref.catalog_digest);if(activity.compatibility_rule_ref.version<1)fail("CarePeriodCompatibilityRuleRequired",row.care_period_ref);}
 refs(row.provision_result_refs);if(row.provision_result_refs.empty()||row.condition_result_refs.size()>65536)fail("CarePeriodActualProvisionRequired",row.care_period_ref);
 for(std::size_t n=0;n<row.condition_result_refs.size();++n){text(row.condition_result_refs[n].result_id);for(std::size_t p=0;p<n;++p)if(row.condition_result_refs[n].result_id==row.condition_result_refs[p].result_id)fail("DuplicateCarePeriodConditionResult",row.care_period_ref);}
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
bool care_period_equal(const CarePeriod& a,const CarePeriod& b){return a.child==b.child&&a.caregiver==b.caregiver&&same(a.place,b.place)&&a.period.from_tick==b.period.from_tick&&a.period.to_tick==b.period.to_tick&&same(a.activity_ref,b.activity_ref)&&same(a.food_support_ref,b.food_support_ref)&&a.outcome==b.outcome;}
bool care_period_inputs_equal(const CarePeriodSource& a,const CarePeriodSource& b){if(a.binding_ref!=b.binding_ref||!same(a.calendar_period_ref,b.calendar_period_ref)||!participation_equal(a.participation,b.participation)||!same(a.actual_complete_source_ref,b.actual_complete_source_ref)||a.provision_result_refs.size()!=b.provision_result_refs.size()||a.condition_result_refs.size()!=b.condition_result_refs.size())return false;for(std::size_t i=0;i<a.provision_result_refs.size();++i)if(!same(a.provision_result_refs[i],b.provision_result_refs[i]))return false;for(std::size_t i=0;i<a.condition_result_refs.size();++i)if(a.condition_result_refs[i].result_id!=b.condition_result_refs[i].result_id)return false;return true;}
Outcome<CarePeriodReadView> read_care_period(const CarePeriodSources& x,const SourceRef& ref,Integer schema){try{
 if(take(care_period_source_schema(ref))!=schema)fail("CarePeriodSourceSchemaMismatch",ref.id);const auto* row=x.tables.periods.find(ref.id);const auto* original=x.tables.sources.find(ref.id);if(!row||!original)fail("CarePeriodOriginalPayloadMissing",ref.id);const auto& calendar=validate_row(x,ref.id,*row,*original);return {true,{row,original,&calendar},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_period_sources(const CarePeriodSources& x){try{
 const auto& c=context(x);check(validate_care_requirement_goal_sources(x.coverage.handovers.acceptance.goals));
 if(x.tables.periods.size()!=x.tables.sources.size())fail("CarePeriodSourceCoverageMismatch");
 for(const auto& pair:x.tables.periods.rows())take(read_care_period(x,care_period_source(pair.first),3));
 for(const auto& pair:x.tables.sources.rows())if(pair.first!=pair.second.care_period_ref||!x.tables.periods.find(pair.first))fail("CarePeriodSourceKeyMismatch",pair.first);
 for(const auto& pair:c.state().facts.rows())if(pair.second.domain=="care_requirements"&&pair.second.kind=="care_period"&&!x.tables.periods.find(pair.first))fail("CarePeriodOriginalPayloadMissing",pair.first);
 struct Reverse final:CarePeriodExpectedSourceVisitor {const CarePeriodSources& x;explicit Reverse(const CarePeriodSources& v):x(v){}
  Status actual_completed_period(const StableId& binding,const MomentOwnerSource& calendar,const MomentOwnerSource& completion)override{std::size_t count=0;for(const auto& pair:x.tables.sources.rows())if(pair.second.binding_ref==binding&&same(pair.second.calendar_period_ref,calendar)&&same(pair.second.actual_complete_source_ref,completion))++count;return count==1?common_success():common_failure("CarePeriodActualCompletedSourceCoverageMismatch",binding,ErrorKind::internal);}
 } reverse(x);check(x.originals.visit_original_completed_care_periods(reverse));return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<CarePeriodApplication> provide_care_period_in_candidate(CommonStore& common,CarePeriodWriteTables tables,const CarePeriodSources& x,CarePeriod row,CarePeriodSource original){try{
 if(&common!=&context(x)||&tables.periods!=&x.tables.periods||&tables.sources!=&x.tables.sources)fail("CarePeriodSameCandidateRequired");
 if(!original.care_period_ref.empty())fail("CarePeriodIdIsOwnerAllocated",original.care_period_ref);check(validate_care_period_shape(row));
 // A genuine retry at a later moment only reads its actual retained result.
 for(const auto& pair:tables.sources.rows())if(pair.second.binding_ref==original.binding_ref&&same(pair.second.calendar_period_ref,original.calendar_period_ref)){
  const auto* prior=tables.periods.find(pair.first);if(!prior||!care_period_equal(*prior,row)||!care_period_inputs_equal(pair.second,original))fail("ConflictingCarePeriodReplay",pair.first);take(read_care_period(x,care_period_source(pair.first),3));return {true,{care_period_source(pair.first),true},{}};
 }
 const auto now=take(x.coverage.handovers.acceptance.goals.clock.read_current_moment());if(!same(now,WorldMoment{row.period.to_tick,{}}))fail("CarePeriodMustPublishAtOriginalCalendarGate");
 check(validate_care_binding_full_calendar_period(x.coverage,original.binding_ref,row.period.from_tick,row.period.to_tick));opening(x,row,original);
 original.care_period_ref=take(common.next_id("care_period"));const auto id=original.care_period_ref;check(validate_care_period_source_shape(original));
 if(!tables.periods.insert(id,std::move(row))||!tables.sources.insert(id,std::move(original)))fail("DuplicateCarePeriod",id);check(common.append_fact({id,"care_period","care_requirements"}));
 take(read_care_period(x,care_period_source(id),3));return {true,{care_period_source(id),false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
