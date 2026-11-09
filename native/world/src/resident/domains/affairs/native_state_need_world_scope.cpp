// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "native_state_need_world_scope.hpp"
#include "resident/kernel/native_complete_world_sources.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
[[noreturn]]void missing(const char* code){throw Failure{common_failure(code,{},ErrorKind::unsupported_capability).error};}
[[noreturn]]void mismatch(const char* code){throw Failure{common_failure(code,{},ErrorKind::internal).error};}
template<class T,class F>Outcome<T> guard(F&& f){try{return {true,f(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("StateNeedWorldScopeResourceExhausted",{},ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("StateNeedWorldScopeResourceExhausted",{},ErrorKind::internal).error};}}
void roots(const WorldBundle& w,const NativeCompleteWorldCoreSources& core,const WorldMoment& at){
 if(&core.characters.early.world!=&w)mismatch("StateNeedWorldScopeCoreMismatch");
 if(!w.catalog||!w.catalog->affairs||!w.catalog->affairs->state_need_policies||!w.affairs.state_needs||
    !w.world_timeline||!w.actor_foundation)missing("StateNeedWorldScopeSelectedRootsRequired");
 if(take(read_state_need_selected_version(w.catalog->identity))!=2)missing("StateNeedWorldScopeExactProfileRequired");
 take(validate_state_need_selected_version(*w.affairs.state_needs,w.catalog->identity,true));
 if(take(compare_world_moments(at,take(w.actual_phase_moment()))))mismatch("StateNeedWorldScopeMomentMismatch");
}
class StateNeedWorldGoalRead final:public StateGoalRead {
 const CareGoalSourceContext* care_;
 const StateGoalRead* medical_;
public:
 StateNeedWorldGoalRead(const CareGoalSourceContext* care,const StateGoalRead* medical):care_(care),medical_(medical){}
 Outcome<StateGoalSnapshot> read_care_goal(const CareCoverageGoal& goal)const override{
  if(!care_)return {false,{},common_failure("CareStateNeedActualGoalSourcesRequired",goal.care_requirement_id,ErrorKind::unsupported_capability).error};
  return read_care_requirement_goal(*care_,goal);
 }
 Outcome<StateGoalSnapshot> read_medical_goal(const MedicalResolutionGoal& goal)const override{
  if(!medical_)return {false,{},common_failure("MedicalStateNeedActualGoalSourcesRequired",goal.medical_need_id,ErrorKind::unsupported_capability).error};
  return medical_->read_medical_goal(goal);
 }
};
class StateNeedWorldScope final:public NativeCompleteWorldSourceScope {
 const WorldBundle& world_;
 const NativeCompleteWorldCoreSources& core_;
 WorldMoment at_;
 WorldCurrentMomentRead clock_;
 CharacterConditionSources body_;
 std::shared_ptr<NativeStateNeedWorldOriginalOwner> owner_;
 NativeStateNeedWorldConstructionContext construction_;
 std::shared_ptr<const NativeStateNeedWorldOriginalScope> original_;
 std::shared_ptr<NativeStateNeedWorldOriginalScope> original_write_;
 std::optional<StateNeedPolicySourceContext> policy_sources_;
 std::optional<NativeStateNeedPolicyRead> policies_;
 std::optional<StateNeedWorldGoalRead> goals_;
 std::optional<StateNeedTaskRead> tasks_;
 std::optional<StateNeedTaskConsumer> task_writer_;
 std::optional<StateNeedReadBinding> read_;
 std::optional<StateNeedWriteBinding> write_;
 NativeCompleteWorldSourceExports exports_;
 StateNeedWorldScope(const WorldBundle& w,const NativeCompleteWorldCoreSources& core,
  std::shared_ptr<NativeStateNeedWorldOriginalOwner> owner,const WorldMoment& at)
  :world_(w),core_(core),at_(at),clock_(w),body_(take(core.characters.family.body().sources())),
   owner_(std::move(owner)),construction_{w,core_,at_,clock_,body_}{}
 void construct(WorldBundle* mutable_world){
  if(!owner_)missing("StateNeedOriginalCareMedicalTaskConstructionRequired");
  if(!owner_->bound_to(world_))mismatch("StateNeedOriginalOwnerCandidateMismatch");
  if(&body_.common!=&world_.common||&body_.state!=&*world_.actor_foundation||
     take(compare_world_moments(body_.actual_now,at_))||take(compare_world_moments(take(clock_.read_current_moment()),at_)))
   mismatch("StateNeedWorldScopeBodyMomentMismatch");
  if(mutable_world){original_write_=take(owner_->write_sources(*mutable_world,construction_));original_=original_write_;}
  else original_=take(owner_->read_sources(construction_));
  if(!original_)missing("StateNeedOriginalOwningScopeRequired");
  if(!original_->bound_to(construction_,mutable_world!=nullptr))mismatch("StateNeedOriginalScopeBindingMismatch");
  const auto& x=original_->sources();
  if(bool(x.care_catalog)!=bool(x.care))mismatch("StateNeedOriginalCareRootPairMismatch");
  if(x.care&&(&x.care->common!=&world_.common||!x.care->rules.bound_to(world_.common)))
   mismatch("StateNeedOriginalCareCandidateMismatch");
  if(x.medical&&!x.medical->bound_to(world_.common))mismatch("StateNeedOriginalMedicalCandidateMismatch");
  if(x.care&&take(compare_world_moments(take(x.care->clock.read_current_moment()),at_)))mismatch("StateNeedOriginalCareMomentMismatch");
  for(const auto& row:world_.catalog->affairs->state_need_policies->policies.rows()){
   if(row.second.goal_kind==0){if(!x.care||!x.care_catalog)missing("StateNeedSelectedCareConstructionRequired");}
   else if(!x.medical||!x.medical_goals)missing("StateNeedSelectedMedicalConstructionRequired");
  }
  policy_sources_.emplace(StateNeedPolicySourceContext{world_.common,world_.catalog->identity,
   *world_.catalog->affairs->state_need_policies,x.task_sources,x.care_catalog,x.care,x.medical});
  policies_.emplace(*policy_sources_);goals_.emplace(x.care,x.medical_goals);
  tasks_.emplace(world_.common,world_.tasks,x.task_sources);
  read_.emplace(StateNeedReadBinding{*policies_,*goals_,*tasks_});exports_.state_need=&*read_;
  if(mutable_world){
   auto* source=original_write_->mutable_task_sources();
   if(!source)missing("StateNeedOriginalTaskSettlementWriterRequired");
   if(source!=&x.task_sources)mismatch("StateNeedOriginalTaskReadWriteRootMismatch");
   task_writer_.emplace(mutable_world->common,mutable_world->tasks,*source,at_);
   write_.emplace(StateNeedWriteBinding{*policies_,*goals_,*task_writer_});exports_.state_need_write=&*write_;
  }
 }
public:
 StateNeedWorldScope(const StateNeedWorldScope&)=delete;
 StateNeedWorldScope&operator=(const StateNeedWorldScope&)=delete;
 StateNeedWorldScope(StateNeedWorldScope&&)=delete;
 StateNeedWorldScope&operator=(StateNeedWorldScope&&)=delete;
 static Outcome<std::shared_ptr<StateNeedWorldScope>> create(const WorldBundle& w,WorldBundle* write,
  const NativeCompleteWorldCoreSources& core,std::shared_ptr<NativeStateNeedWorldOriginalOwner> owner,const WorldMoment& at){
  return guard<std::shared_ptr<StateNeedWorldScope>>([&]{roots(w,core,at);
   if(write&&write!=&w)mismatch("StateNeedMutableCandidateMismatch");
   auto out=std::unique_ptr<StateNeedWorldScope>(new StateNeedWorldScope(w,core,std::move(owner),at));
   out->construct(write);return checkpoint_share(std::move(out));});
 }
 bool bound_to(const WorldBundle& w)const noexcept override{return &world_==&w;}
 NativeCompleteWorldRoute route()const noexcept override{return NativeCompleteWorldRoute::state_need;}
 const WorldMoment& actual_at()const noexcept override{return at_;}
 const NativeCompleteWorldSourceExports& sources()const noexcept override{return exports_;}
};
}
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>> make_native_state_need_world_read_scope(
 const WorldBundle& w,const NativeCompleteWorldCoreSources& core,std::shared_ptr<NativeStateNeedWorldOriginalOwner> originals,const WorldMoment& at){
 auto built=StateNeedWorldScope::create(w,nullptr,core,std::move(originals),at);
 if(!built.ok)return {false,{},std::move(built.error)};return {true,std::move(built.value),{}};
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>> make_native_state_need_world_write_scope(
 WorldBundle& w,const NativeCompleteWorldCoreSources& core,std::shared_ptr<NativeStateNeedWorldOriginalOwner> originals,const WorldMoment& at){
 auto built=StateNeedWorldScope::create(w,&w,core,std::move(originals),at);
 if(!built.ok)return {false,{},std::move(built.error)};return {true,std::move(built.value),{}};
}
}
