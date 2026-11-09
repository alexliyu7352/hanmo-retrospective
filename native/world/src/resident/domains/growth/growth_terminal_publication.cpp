#include "resident/domains/growth/growth_model.hpp"
#include "growth_terminal_publication.hpp"
#include "growth_action_time_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include <charconv>
namespace hanmo::resident {
namespace {
struct Failure{RuleError error;};
[[noreturn]] void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
template<class T,class F>Outcome<T> guarded(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
bool allocated(const CommonState& c,const StableId& id){constexpr char p[]="growth_terminal:";if(id.rfind(p,0))return false;const char* begin=id.data()+sizeof(p)-1;Integer n=0;const auto r=std::from_chars(begin,id.data()+id.size(),n);return r.ec==std::errc{}&&r.ptr==id.data()+id.size()&&n>0&&n<c.next_instance_id&&id==StableId(p)+std::to_string(n);}
LifeFixedOutcomeRef actual_outcome(const GrowthState& state,const StableId& id){LifeFixedOutcomeRef r;r.id=id;const auto* fact=state.facts.find(id);const auto* life=state.life_activities.find(id);if(bool(fact)==bool(life))fail("GrowthTerminalOwnerAmbiguous",id);if(life){r.kind=LifeFixedOutcomeRef::Kind::life_activity;return r;}if(std::holds_alternative<TrainingFact>(*fact))r.kind=LifeFixedOutcomeRef::Kind::training;else if(std::holds_alternative<StudyFact>(*fact))r.kind=LifeFixedOutcomeRef::Kind::study;else if(std::holds_alternative<RestFact>(*fact))r.kind=LifeFixedOutcomeRef::Kind::rest;else fail("GrowthTerminalOriginalResultRequired",id);return r;}
bool same_result(const LifeFixedOutcomeRef& a,const LifeFixedOutcomeRef& b){return a.kind==b.kind&&a.id==b.id&&a.period_ordinal==b.period_ordinal&&a.secondary_ref==b.secondary_ref;}
GrowthTerminalPublicationView check(const CommonStore& common,const GrowthState& state,const GrowthActionTimeRead& reader,const ActionTimeRegistration& reg,const GrowthTerminalPublication& row,bool require_header){
 if(!allocated(common.state(),row.publication_id)||row.action_id.empty()||row.terminal_source_schema!=(reg.action_kind=="trade"?2:1))fail("GrowthTerminalPublicationShapeInvalid",row.publication_id);
 take(reader.validate_adopted_terminal_source(reg,row.action_id));auto time=take(read_growth_original_time(reader,reg,row.action_id));if(!time.terminal||!time.ended_at||!time.terminal_source||!same_source_ref(*time.terminal_source,row.terminal_source_ref)||take(compare_world_moments(*time.ended_at,row.occurred_at))||!same_result(actual_outcome(state,row.action_id),row.original_result_ref))fail("GrowthTerminalPublicationSourceMismatch",row.publication_id);
 const auto* original=common.state().facts.find(row.action_id);if(!original)fail("GrowthTerminalOriginalHeaderMissing",row.action_id);
 if(require_header){const auto* h=common.state().facts.find(row.publication_id);if(!h||h->id!=row.publication_id||h->kind!="growth_result_terminal"||h->domain!="growth")fail("GrowthTerminalPublicationHeaderMismatch",row.publication_id);const auto original_position=take(read_actual_common_fact_position(common,*original));const auto publication_position=take(read_actual_common_fact_position(common,*h));if(original_position>=publication_position)fail("GrowthTerminalPublicationOrderMismatch",row.publication_id);}
 return {&row,std::move(time.actor_id),std::move(time.action_kind),take(reader.read_original_business_rule(reg,row.action_id))};
}
}
Outcome<GrowthTerminalPublicationView> read_growth_terminal_publication(const CommonStore& common,const GrowthState& state,const GrowthActionTimeRead& reader,const ActionTimeRegistration& reg,const StableId& id){return guarded<GrowthTerminalPublicationView>([&]{const auto* row=state.terminal_publications.find(id);if(!row||row->publication_id!=id)fail("GrowthTerminalPublicationMissing",id);for(const auto& other:state.terminal_publications.rows())if(other.first!=id&&other.second.action_id==row->action_id)fail("DuplicateGrowthTerminalPublication",row->action_id);return check(common,state,reader,reg,*row,true);});}
Outcome<StableId> append_growth_terminal_publication_in_candidate(CommonStore& common,GrowthState& state,const GrowthActionTimeRead& reader,const ActionTimeRegistration& reg,const StableId& id){return guarded<StableId>([&]{const GrowthTerminalPublication* existing=nullptr;for(const auto& row:state.terminal_publications.rows())if(row.second.action_id==id){if(existing)fail("DuplicateGrowthTerminalPublication",id);existing=&row.second;}if(existing){take(read_growth_terminal_publication(common,state,reader,reg,existing->publication_id));return existing->publication_id;}
 take(reader.validate_adopted_terminal_source(reg,id));auto time=take(read_growth_original_time(reader,reg,id));if(!time.terminal||!time.ended_at||!time.terminal_source)fail("GrowthOriginalTerminalRequired",id);
 GrowthTerminalPublication row;row.publication_id=take(common.next_id("growth_terminal"));row.action_id=id;row.original_result_ref=actual_outcome(state,id);row.terminal_source_ref=*time.terminal_source;row.terminal_source_schema=reg.action_kind=="trade"?2:1;row.occurred_at=*time.ended_at;check(common,state,reader,reg,row,false);const auto publication=row.publication_id;if(!state.terminal_publications.insert(publication,std::move(row)))fail("GrowthTerminalPublicationCollision",publication);take(common.append_fact({publication,"growth_result_terminal","growth"}));take(read_growth_terminal_publication(common,state,reader,reg,publication));return publication;
 });}
Status validate_growth_terminal_publications(const CommonStore& common,const GrowthState& state,const GrowthActionTimeRead& reader,const std::vector<ActionTimeRegistration>& selected){return guarded<Done>([&]{
 struct Sink final:ActionTimeOwnerIdSink {
  const CommonStore& common;const GrowthState& state;const GrowthActionTimeRead& reader;const ActionTimeRegistration& registration;
  Sink(const CommonStore& c,const GrowthState& s,const GrowthActionTimeRead& r,const ActionTimeRegistration& selected):common(c),state(s),reader(r),registration(selected){}
  Status append(std::string_view id)override{return guarded<Done>([&]{
   auto proof=take(reader.read_action_time_owner_borrowed(registration,id));
   std::size_t count=0;for(const auto& row:state.terminal_publications.rows())if(row.second.action_id==id){take(read_growth_terminal_publication(common,state,reader,registration,row.first));++count;}
   if(count!=(proof.phase==ActionTimeOwnerPhase::terminal?1u:0u))fail("GrowthTerminalPublicationCoverageMismatch",proof.action_ref);
   return Done{};
  });}
 };
 for(std::size_t i=0;i<selected.size();++i){const auto& reg=selected[i];if(reg.owner_domain!="growth")continue;for(std::size_t j=0;j<i;++j)if(selected[j].owner_domain=="growth"&&selected[j].action_kind==reg.action_kind)fail("DuplicateGrowthTerminalRegistration",reg.action_kind);Sink sink(common,state,reader,reg);take(reader.enumerate_action_time_owners(reg,sink));}
 // 反向覆盖沿原来源联合分派，交易不再拿life_activity ID查FSL表。
 for(const auto& row:state.terminal_publications.rows()){
  bool covered=false;
  for(const auto& reg:selected)if(reg.owner_domain=="growth"){
   bool matches=false;
   if(row.second.terminal_source_schema==2&&row.second.terminal_source_ref.domain=="growth"&&row.second.terminal_source_ref.kind=="life_activity"){
    const auto* life=state.life_activities.find(row.second.terminal_source_ref.id);
    matches=life&&life->view.kind==LifeActivityKind::trade&&reg.action_kind=="trade";
   }else if(row.second.terminal_source_schema==1&&row.second.terminal_source_ref.domain=="growth"&&row.second.terminal_source_ref.kind=="growth_fixed_service_boundary"){
    const auto* boundary=state.fixed_service_boundaries.find(row.second.terminal_source_ref.id);
    matches=boundary&&boundary->action_kind==reg.action_kind;
   }
   if(matches){if(covered)fail("DuplicateGrowthTerminalRegistration",row.first);take(read_growth_terminal_publication(common,state,reader,reg,row.first));covered=true;}
  }
  if(!covered)fail("GrowthTerminalPublicationRegistrationMissing",row.first);
 }
 for(const auto& row:common.state().facts.rows())if(row.second.kind=="growth_result_terminal"&&(row.second.domain!="growth"||!state.terminal_publications.find(row.first)))fail("OrphanGrowthTerminalPublicationHeader",row.first);return Done{};
 });}
}
