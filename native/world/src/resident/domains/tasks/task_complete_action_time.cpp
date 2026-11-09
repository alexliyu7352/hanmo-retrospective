#include "task_complete_action_time.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> required(const StableId& id={}){return {false,{},common_failure("TaskWorkWorldReadFactoryRequired",id,ErrorKind::unsupported_capability).error};}
class CompleteTaskActionTimeRead final:public ActionTimeOriginalSourceRead {
 const WorldBundle& world_;TaskWorkWorldFactory factory_;const CharacterConditionWorldRead* body_;
 const CompleteExactWorldBindingOwner* owner_;
 template<class T,class F>Outcome<T> fixed(F f)const{
  if(!world_.catalog||!world_.catalog->tasks||!world_.catalog->exact_time||!world_.action_elapsed||!world_.tasks.exact_execution)return {false,{},common_failure("TaskExactWorldSourcesRequired",{},ErrorKind::internal).error};
  if(world_.tasks.exact_execution->version==1){TaskActionTimeSourceRead source(world_.common,world_.tasks,*world_.catalog->tasks,world_.catalog->exact_time->scale_ref,world_.catalog->identity);return f(source);}
  if(world_.tasks.exact_execution->version!=2||!body_)return {false,{},common_failure("TaskExactOriginalLifeSourcesRequired",{},ErrorKind::unsupported_capability).error};
  auto body=body_->sources();if(!body.ok)return {false,{},body.error};auto now=world_.actual_phase_moment();if(!now.ok)return {false,{},now.error};
  if(&body.value.common!=&world_.common||body.value.deaths!=&world_.family_life||!body.value.life_sources||
     body.value.elapsed!=&*world_.action_elapsed)return {false,{},common_failure("TaskExactLifeCandidateMismatch",{},ErrorKind::internal).error};
  auto at=compare_world_moments(body.value.actual_now,now.value);if(!at.ok)return {false,{},at.error};if(at.value)return {false,{},common_failure("TaskExactLifeMomentMismatch",{},ErrorKind::internal).error};
  TaskExactLifeSources life{world_.common,world_.family_life,*body.value.life_sources,body.value.actual_now};
  TaskActionTimeSourceRead source(world_.common,world_.tasks,*world_.catalog->tasks,world_.catalog->exact_time->scale_ref,world_.catalog->identity,{},&life);return f(source);
 }
 template<class T,class F>Outcome<T> supervised(F f)const{
  if(!owner_&&!factory_.read)return required<T>();auto at=world_.actual_phase_moment();if(!at.ok)return {false,{},at.error};
  auto lease=owner_?owner_->read_task_work(at.value):factory_.read(world_,at.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return required<T>();auto binding=lease.value->bindings();
  if(&binding.execution.common!=&world_.common||&binding.execution.tasks!=&world_.tasks)return {false,{},common_failure("TaskWorkActionTimeCandidateMismatch",{},ErrorKind::internal).error};
  SupervisionActionTimeRead source(binding.execution);return f(source);
 }
 template<class T,class F>Outcome<T> registered(const ActionTimeRegistration& r,F f)const{
  if(r.owner_domain!="tasks")return {false,{},common_failure("TaskActionTimeOwnerMismatch",r.owner_domain).error};
  if(supervision_action_kind(r.action_kind))return supervised<T>(f);
  if(exact_task_action_kind(r.action_kind))return fixed<T>(f);
  return {false,{},common_failure("UnregisteredTaskActionTimeKind",r.action_kind,ErrorKind::unsupported_capability).error};
 }
public:
 CompleteTaskActionTimeRead(const WorldBundle& w,TaskWorkWorldFactory f,const CharacterConditionWorldRead*b,const CompleteExactWorldBindingOwner* owner):world_(w),factory_(f),body_(b),owner_(owner){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&r,ActionTimeOwnerIdSink&sink)const override{return registered<Done>(r,[&](const auto&s){return s.enumerate_action_time_owners(r,sink);});}
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&r,const StableId&id)const override{return registered<ActionTimeOwnerProof>(r,[&](const auto&s){return s.read_action_time_owner(r,id);});}
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource& ref)const override{
  const auto* m=std::get_if<MomentOwnerSource>(&ref);if(!m||m->source_ref.domain!="tasks")return {false,{},common_failure("TaskActionTimeMomentSourceRequired").error};
  if(supervision_source_kind(m->source_ref.kind))return supervised<WorldMoment>([&](const auto&s){return s.read_action_time_source(ref);});
  return fixed<WorldMoment>([&](const auto&s){return s.read_action_time_source(ref);});
 }
 Status validate_fixed_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const ActionElapsedState&e,const std::vector<FixedServiceTransition>&v)const override{return registered<Done>(r,[&](const auto&s){return s.validate_fixed_action_sources(r,p,e,v);});}
 Status validate_shared_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const SharedWorkElapsedState&e)const override{return registered<Done>(r,[&](const auto&s){return s.validate_shared_action_sources(r,p,e);});}
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&m)const override{return registered<RunStop>(r,[&](const auto&s){return s.read_action_continuation_stop(r,p,m);});}
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&m)const override{return registered<OriginalActionRemaining>(r,[&](const auto&s){return s.read_original_action_remaining(r,p,m);});}
 Outcome<ExhaustedAuthorizationWindow> read_exhausted_authorization_window(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&m)const override{return registered<ExhaustedAuthorizationWindow>(r,[&](const auto&s){return s.read_exhausted_authorization_window(r,p,m);});}
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override{return {false,{},common_failure("TaskActionHasNoSharedGate").error};}
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&id)const override{return {false,{},common_failure("TaskActionHasNoSharedGate",id).error};}
 Status validate_original_gate_result(const SharedWorkGateResult&)const override{return common_failure("TaskActionHasNoSharedGate");}
};
}
namespace {
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_complete_task_action_time_reader_bound(const WorldBundle&w,TaskWorkWorldFactory f,const CharacterConditionWorldRead*body,const CompleteExactWorldBindingOwner* owner){
 if(!w.catalog||!w.catalog->exact_time||!w.catalog->tasks)return {false,{},common_failure("TaskActionTimeCatalogRequired",{},ErrorKind::internal).error};
 if(w.tasks.work_execution&&!owner&&(!f.read||!f.write))return required<std::shared_ptr<const ActionTimeOriginalSourceRead>>();
 if(w.tasks.exact_execution&&w.tasks.exact_execution->version==2&&!body)return {false,{},common_failure("TaskExactOriginalLifeSourcesRequired",{},ErrorKind::unsupported_capability).error};
 return {true,checkpoint_make_shared<CompleteTaskActionTimeRead>(w,f,body,owner),{}};
}
}
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_complete_task_action_time_reader(const WorldBundle&w,TaskWorkWorldFactory f,const CharacterConditionWorldRead*body){return make_complete_task_action_time_reader_bound(w,f,body,nullptr);}
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_complete_task_action_time_reader_in_owner(const WorldBundle&w,const CompleteExactWorldBindingOwner&owner,const CharacterConditionWorldRead*body){return make_complete_task_action_time_reader_bound(w,{},body,&owner);}
}
