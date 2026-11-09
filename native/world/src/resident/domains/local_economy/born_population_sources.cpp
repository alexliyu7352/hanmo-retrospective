#include "born_population_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <charconv>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> result){if(!result.ok)throw Failure{std::move(result.error)};return std::move(result.value);}
void take(Status result){if(!result.ok)throw Failure{std::move(result.error)};}
template<class T,class F>Outcome<T> guarded(F&& f){try{return {true,f(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
bool same_source(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool source_kind(const SourceRef& s){return s.domain=="local_economy"&&s.kind=="named_birth_population_transition";}
bool birth_kind(const SourceRef& s){return s.domain=="family_birth"&&s.kind=="family_birth_recorded";}
bool allocated(const CommonStore& c,const StableId& id){
 constexpr std::string_view prefix="named_birth_population_transition:";
 if(id.compare(0,prefix.size(),prefix)!=0)return false;
 const auto raw=std::string_view(id).substr(prefix.size());Integer number=0;
 if(raw.empty()||raw.front()=='0')return false;
 const auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),number);
 return parsed.ec==std::errc{}&&parsed.ptr==raw.data()+raw.size()&&number>0&&number<c.state().next_instance_id;
}
WorldMoment bindings(const BornPopulationSourceContext& x){
 if(!named_birth_population_selected(x.selected_local_economy)||!x.catalog.enabled||
    &x.common!=&x.births.common||!x.births.catalog.enabled||x.births.catalog.selected_version!=4||
    !x.births.access.people||!x.births.access.people->bound_to(x.common))fail("NamedBirthPopulationSourcesRequired");
 auto now=take(x.clock.read_current_moment());
 if(compare(now,take(x.births.access.people->read_current_moment())))fail("NamedBirthPopulationClockMismatch");
 return now;
}
const NamedBirthPopulationTransition* read(const BornPopulationSourceContext& x,const SourceRef& source,Integer schema){
 auto now=bindings(x);
 if(schema!=1||!source_kind(source))fail("NamedBirthPopulationSourceSchemaMismatch",source.id);
 const auto* row=x.economy.named_birth_population_transitions.find(source.id);
 const auto* header=x.common.state().facts.find(source.id);
 if(!row||row->id!=source.id||!allocated(x.common,row->id)||!header||header->id!=row->id||
    header->domain!=source.domain||header->kind!=source.kind||!birth_kind(row->birth_fact_ref))
  fail("NamedBirthPopulationSourceIdentityMismatch",source.id);
 const auto birth=take(read_born_character_source(x.births,BirthFactRef{row->birth_fact_ref.id}));
 if(!birth.birth||!birth.original_fact||birth.original_fact->id!=row->birth_fact_ref.id||
    birth.birth->actor_id!=row->child_actor_id||compare(row->occurred_at,birth.occurred_at)||
    compare(row->occurred_at,now)>0)fail("NamedBirthPopulationOriginalBirthMismatch",row->id);
 const auto at=take(read_actual_common_fact_position(x.common,source));
 if(take(read_actual_common_fact_position(x.common,row->birth_fact_ref))>=at||
    take(read_actual_common_fact_position(x.common,{"family_birth","actor_creation",row->child_actor_id}))>=at)
  fail("NamedBirthPopulationSourceOrderMismatch",row->id);
 for(const auto& other:x.economy.named_birth_population_transitions.rows()){
  if(other.first==source.id){if(&other.second!=row)fail("NamedBirthPopulationRetainedIdentityMismatch",source.id);continue;}
  if(same_source(other.second.birth_fact_ref,row->birth_fact_ref)||other.second.child_actor_id==row->child_actor_id)
   fail("DuplicateNamedBirthPopulationTransition",row->id);
 }
 return row;
}
}
bool named_birth_population_selected(Integer selected)noexcept{return selected>=11&&selected<=20;}
Status validate_named_birth_population_selected_layout(const LocalEconomyState& state,Integer selected){
 if(selected<0||selected>20)return common_failure("UnsupportedLocalEconomyCodecVersion");
 if(!named_birth_population_selected(selected)&&!state.named_birth_population_transitions.empty())return common_failure("NamedBirthPopulationSelectedSourceRequired");
 return common_success();
}
Status validate_born_population_source_bindings(const BornPopulationSourceContext& x){return guarded<Done>([&]{bindings(x);return Done{};});}
Outcome<const NamedBirthPopulationTransition*> read_named_birth_population_transition(const BornPopulationSourceContext& x,const SourceRef& source,Integer schema){return guarded<const NamedBirthPopulationTransition*>([&]{return read(x,source,schema);});}
Status validate_named_birth_population_transitions(const BornPopulationSourceContext& x){return guarded<Done>([&]{
 bindings(x);
 for(const auto& row:x.economy.named_birth_population_transitions.rows()){
  if(row.first!=row.second.id)fail("NamedBirthPopulationSourceIdentityMismatch",row.first);
  if(read(x,{"local_economy","named_birth_population_transition",row.first},1)!=&row.second)fail("NamedBirthPopulationRetainedIdentityMismatch",row.first);
 }
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="local_economy"&&row.second.kind=="named_birth_population_transition"){
  if(row.first!=row.second.id||!x.economy.named_birth_population_transitions.find(row.first))fail("MissingNamedBirthPopulationPayload",row.first);
 }
 return Done{};
});}
Outcome<const NamedBirthPopulationTransition*> record_named_birth_population_transition_in_candidate(CommonStore& common,LocalEconomyState& economy,const BornPopulationSourceContext& x,const BirthFactRef& requested){return guarded<const NamedBirthPopulationTransition*>([&]{
 if(&common!=&x.common||&economy!=&x.economy)fail("NamedBirthPopulationCandidateMismatch");
 auto now=bindings(x);
 const auto birth=take(read_born_character_source(x.births,requested));
 if(!birth.birth||!birth.original_fact)fail("NamedBirthPopulationOriginalBirthRequired",requested.birth_fact_id);
 take(validate_named_birth_population_transitions(x));
 const SourceRef original{"family_birth","family_birth_recorded",requested.birth_fact_id};
 for(const auto& row:economy.named_birth_population_transitions.rows())if(same_source(row.second.birth_fact_ref,original)||row.second.child_actor_id==birth.birth->actor_id){
  if(!same_source(row.second.birth_fact_ref,original)||row.second.child_actor_id!=birth.birth->actor_id)fail("NamedBirthPopulationConflict",row.first);
  return read(x,{"local_economy","named_birth_population_transition",row.first},1);
 }
 // A historical omission cannot be repaired using today's actor projection.
 if(compare(now,birth.occurred_at)||compare(now,WorldMoment{birth.birth->tick,{}}))fail("NamedBirthPopulationAtActualBirthRequired",requested.birth_fact_id);
 const auto id=take(common.next_id("named_birth_population_transition"));
 NamedBirthPopulationTransition row{id,original,birth.birth->actor_id,birth.occurred_at};
 take(common.append_fact({id,"named_birth_population_transition","local_economy"}));
 economy.named_birth_population_transitions.insert(id,std::move(row));
 return read(x,{"local_economy","named_birth_population_transition",id},1);
});}
}
