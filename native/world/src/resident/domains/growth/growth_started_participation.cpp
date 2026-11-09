#include "growth_started_participation.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <array>
#include <string_view>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool owned(const LifeOriginalRef& ref,const SourceRef& source,std::string_view field){const auto* p=std::get_if<LifeOwnedFieldRef>(&ref);return p&&same_source_ref(p->owner_ref,source)&&p->field_selector==field;}
}
bool growth_start_participation_consumer_selected(const ActorFoundationCatalog& catalog)noexcept{
 for(const auto& module:catalog.identity.module_versions)if(module.first=="care_requirements"&&module.second=="6")return true;return false;
}
Status validate_growth_start_participation_writer(const CommonStore& common,const GrowthState& growth,const ActorFoundationCatalog& catalog,const GrowthStartedParticipationWrite* writer){
 if(growth_start_participation_consumer_selected(catalog)&&(!writer||!writer->bound_to(common,growth)))return common_failure("GrowthOriginalParticipationWriterRequired",{},ErrorKind::unsupported_capability);
 return common_success();
}
bool is_growth_original_start_header(const FactRecord& h)noexcept{
 return (h.domain=="cultivation"&&(h.kind=="training"||h.kind=="skill_study"))||
  (h.domain=="services"&&h.kind=="long_rest")||(h.domain=="growth"&&h.kind=="life_activity");
}
Outcome<std::optional<GrowthStartedParticipationRead>> read_growth_started_participation(const CommonStore& common,const GrowthState& growth,const MomentOwnerSource& source,const StableId& actor,const WorldMoment& now){try{
 const auto original=take(read_growth_started_business_identity(common,growth,source,now));
 const auto& started=*original.actual_start;const auto& command=*original.actual_command;
 const auto* header=common.state().facts.find(source.source_ref.id);
 if(!header||!is_growth_original_start_header(*header)||std::count(common.state().facts.order().begin(),common.state().facts.order().end(),header->id)!=1)fail("GrowthParticipationOriginalHeaderRequired",source.source_ref.id);
 const auto& b=command.command_binding;
 auto memory=CheckpointMemoryCharge(0);
 const auto* cached=common.state().results.find(command.actor_id+":"+std::to_string(b.request_seq));
 if(b.request_seq<1||!cached||!*cached||!(*cached)->ok||!(*cached)->details||(*cached)->result_id!=b.result_id||(*cached)->kind!=b.command_kind||(*cached)->tick!=started.occurred_at.half_day_tick||(*cached)->revision<1||(*cached)->revision>common.revision())fail("GrowthParticipationSealedStartRequired",header->id);
 const auto* result=std::get_if<ActionStarted>(&(*cached)->details->data);
 if(!result||result->action_id!=started.action_ref)fail("GrowthParticipationActualActionResultMismatch",header->id);
 std::size_t history=0;for(const auto& h:common.state().history)if(h.result_id==b.result_id){++history;if(h.actor_id!=command.actor_id||h.kind!=b.command_kind||h.tick!=(*cached)->tick)fail("GrowthParticipationOriginalHistoryMismatch",header->id);}if(history!=1)fail("GrowthParticipationOriginalHistoryRequired",header->id);
 struct Person{const StableId* actor=nullptr;std::string_view role;};std::array<Person,2> expected{};std::size_t count=0;
 auto add=[&](const StableId& id,std::string_view role){if(id.empty()||!common.actor(id)||count==expected.size())fail("GrowthParticipationOriginalActorRequired",header->id);for(std::size_t i=0;i<count;++i)if(*expected[i].actor==id)fail("GrowthParticipationDuplicateActor",header->id);expected[count++]={&id,role};};
 if(const auto* raw=growth.facts.find(header->id)){
  if(const auto* training=std::get_if<TrainingFact>(raw)){add(training->training.student_id,"student");if(!training->training.teacher_id.empty())add(training->training.teacher_id,"teacher");}
  else if(const auto* study=std::get_if<StudyFact>(raw)){add(study->terms.student_id,"student");add(study->terms.teacher_id,"teacher");}
  else if(const auto* rest=std::get_if<RestFact>(raw))add(rest->actor_id,"recipient");
  else fail("GrowthParticipationOriginalPayloadRequired",header->id);
 }else{
  const auto* life=growth.life_activities.find(header->id);
  // Trade's still-unbound settlement branch is not an adopted service start.
  if(!life||life->view.kind!=LifeActivityKind::service||*original.action_kind!="service"||life->view.participant_actor_ids.size()!=2||life->view.participant_actor_ids[0]!=life->view.actor_id||life->view.participant_actor_ids[1]!=life->view.provider_actor_id)fail("GrowthParticipationActualServiceRequired",header->id,ErrorKind::unsupported_capability);
  add(life->view.actor_id,"recipient");add(life->view.provider_actor_id,"provider");
 }
 if(started.input_snapshot.actors.size()!=count)fail("GrowthParticipationStartCoverageMismatch",header->id);
 const LifeFixedActorInputs* selected=nullptr;
 for(std::size_t n=0;n<count;++n){const auto& input=started.input_snapshot.actors[n];std::size_t match=0;for(std::size_t i=0;i<count;++i)if(input.actor_id==*expected[i].actor&&input.role==expected[i].role)++match;
  if(match!=1||(n&&started.input_snapshot.actors[n-1].actor_id>=input.actor_id)||input.participation_source_refs.size()!=1||!owned(input.participation_source_refs.front(),source.source_ref,"participant_actor_ids")||input.occupation_source_refs.size()!=1||!owned(input.occupation_source_refs.front(),source.source_ref,"action_id"))fail("GrowthParticipationOriginalSetMismatch",header->id);
  if(input.actor_id==actor)selected=&input;
 }
 if(!selected)return {true,std::nullopt,{}};
 return {true,GrowthStartedParticipationRead{header,&started,selected},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<GrowthStartedParticipationRead>> read_growth_started_participation_at_header(const CommonStore& common,const GrowthState& growth,const FactRecord& h,const StableId& actor,const WorldMoment& now){
 if(common.state().facts.find(h.id)!=&h||!is_growth_original_start_header(h))return {false,{},common_failure("GrowthParticipationOriginalHeaderRequired",h.id,ErrorKind::unsupported_capability).error};
 auto memory=CheckpointMemoryCharge(0);
 return read_growth_started_participation(common,growth,{{"growth",h.kind,h.id},1},actor,now);
}
}
