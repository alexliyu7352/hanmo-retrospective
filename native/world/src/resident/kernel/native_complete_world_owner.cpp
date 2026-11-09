#include "native_complete_world_owner.hpp"
#include "world_bundle.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <new>
#include <stdexcept>
#include <utility>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T> guarded(F&& f){try{return {true,f(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("NativeCompleteOwnerResourceExhausted",{},ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("NativeCompleteOwnerResourceExhausted",{},ErrorKind::internal).error};}}
void roots(const WorldBundle& w){
 if(!w.catalog||!w.catalog->exact_time||!exact_complete_selected(*w.catalog->exact_time)||
    !w.catalog->complete_exact_time_runtime||!w.world_timeline||!w.action_elapsed||
    !w.facility_claims||!w.movement_exact||!w.actor_foundation||!w.catalog->actor_foundation)
  throw Failure{common_failure("NativeCompleteWorldSelectedRootsRequired",{},ErrorKind::unsupported_capability).error};
}
void graph(const WorldBundle& w,const NativeCompleteWorldSourceOwner* p,bool write){
 if(!p)throw Failure{common_failure("NativeCompleteWorldSourceOwnerRequired",{},ErrorKind::unsupported_capability).error};
 if(!p->bound_to(w))throw Failure{common_failure("NativeCompleteWorldSourceOwnerMismatch",{},ErrorKind::internal).error};
 take(p->validate_bindings(w,write));const auto& b=p->reads();
 if(&b.runtime!=w.catalog->complete_exact_time_runtime.get()||&b.facility_claims!=&*w.facility_claims||&b.movement!=&*w.movement_exact)
  throw Failure{common_failure("NativeCompleteWorldReadRootMismatch",{},ErrorKind::internal).error};
 auto body=take(b.character_conditions.sources());
 if(&body.common!=&w.common||&body.state!=&*w.actor_foundation||
    &body.catalog!=w.catalog->actor_foundation.get()||
    take(compare_world_moments(body.actual_now,take(w.actual_phase_moment()))))
  throw Failure{common_failure("NativeCompleteWorldBodyRootMismatch",{},ErrorKind::internal).error};
}
}
NativeCompleteWorldBindingOwner::NativeCompleteWorldBindingOwner(const WorldBundle& w,WorldBundle* mutable_world,NativeCompleteWorldFactoryRegistry factories,std::unique_ptr<NativeCompleteWorldSourceOwner> source)
 :world_(w),writable_(mutable_world),factories_(factories),sources_(std::move(source)){}
NativeCompleteWorldBindingOwner::~NativeCompleteWorldBindingOwner()=default;
Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>> NativeCompleteWorldBindingOwner::create(const WorldBundle& w,const NativeCompleteWorldFactoryRegistry& f){return guarded<std::unique_ptr<CompleteExactWorldBindingOwner>>([&]{
 roots(w);take(validate_native_complete_world_factory_routes(f));
 if(!f.sources_read)throw Failure{common_failure("NativeCompleteReadSourceGraphConstructionRequired",{},ErrorKind::unsupported_capability).error};
 auto owner=std::unique_ptr<NativeCompleteWorldBindingOwner>(new NativeCompleteWorldBindingOwner(w,nullptr,f,{}));
 owner->sources_=take(f.sources_read(w,*owner));graph(w,owner->sources_.get(),false);
 owner->ready_=true;return std::unique_ptr<CompleteExactWorldBindingOwner>(std::move(owner));
});}
Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>> NativeCompleteWorldBindingOwner::create(WorldBundle& w,const NativeCompleteWorldFactoryRegistry& f){return guarded<std::unique_ptr<CompleteExactWorldBindingOwner>>([&]{
 roots(w);take(validate_native_complete_world_factory_routes(f));
 if(!f.sources_write)throw Failure{common_failure("NativeCompleteWriteSourceGraphConstructionRequired",{},ErrorKind::unsupported_capability).error};
 auto owner=std::unique_ptr<NativeCompleteWorldBindingOwner>(new NativeCompleteWorldBindingOwner(w,&w,f,{}));
 owner->sources_=take(f.sources_write(w,*owner));graph(w,owner->sources_.get(),true);
 owner->ready_=true;return std::unique_ptr<CompleteExactWorldBindingOwner>(std::move(owner));
});}
const CompleteExactWorldReadBindings& NativeCompleteWorldBindingOwner::reads()const{
 if(!ready_||!sources_)throw Failure{common_failure("NativeCompleteOwnerConstructionNotPublished",{},ErrorKind::internal).error};
 return sources_->reads();
}
NativeCompleteWorldWriterExports NativeCompleteWorldBindingOwner::writer_exports(){
 if(!ready_||!sources_)throw Failure{common_failure("NativeCompleteOwnerConstructionNotPublished",{},ErrorKind::internal).error};
 return writable_?sources_->writers():NativeCompleteWorldWriterExports{};
}
Status NativeCompleteWorldBindingOwner::actual(const WorldMoment& at)const{
 if(!ready_||!sources_)return common_failure("NativeCompleteOwnerConstructionNotPublished",{},ErrorKind::internal);
 auto now=world_.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
 auto same=compare_world_moments(now.value,at);if(!same.ok)return {false,{},std::move(same.error)};
 if(same.value)return common_failure("NativeCompleteAcquisitionMomentMismatch",{},ErrorKind::internal);
 if(!sources_->bound_to(world_))return common_failure("NativeCompleteAcquisitionWorldMismatch",{},ErrorKind::internal);
 return common_success();
}
Status NativeCompleteWorldBindingOwner::mutable_actual(const WorldMoment& at)const{
 if(!writable_||writable_!=&world_)return common_failure("NativeCompleteMutableOwnerRequired",{},ErrorKind::unsupported_capability);
 return actual(at);
}
Status NativeCompleteWorldBindingOwner::capture(const WorldMoment& at,const ExactPhaseWriteContext& writes)const{
 auto valid=mutable_actual(at);if(!valid.ok)return valid;
 if(writable_->active_exact_write_context()!=&writes)return common_failure("NativeCompleteSameCaptureRequired",{},ErrorKind::internal);
 return common_success();
}
Outcome<std::shared_ptr<const EconomicExactReadLease>> NativeCompleteWorldBindingOwner::read_economic(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.economic_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<EconomicExactWriteLease>> NativeCompleteWorldBindingOwner::write_economic(const WorldMoment& at,const DynamicStepInput* phase){
 auto valid=mutable_actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.economic_write(*writable_,*sources_,at,phase);
}
Outcome<std::shared_ptr<const CraftExactReadLease>> NativeCompleteWorldBindingOwner::read_craft(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.craft_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<CraftExactWriteLease>> NativeCompleteWorldBindingOwner::write_craft(const WorldMoment& at,ExactPhaseWriteContext& writes){
 auto valid=capture(at,writes);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.craft_write(*writable_,*sources_,at,writes);
}
Outcome<std::shared_ptr<const RepairExactReadLease>> NativeCompleteWorldBindingOwner::read_repair(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.repair_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<RepairExactWriteLease>> NativeCompleteWorldBindingOwner::write_repair(const WorldMoment& at,ExactPhaseWriteContext& writes){
 auto valid=capture(at,writes);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.repair_write(*writable_,*sources_,at,writes);
}
Outcome<std::shared_ptr<const CraftFacilityReadLease>> NativeCompleteWorldBindingOwner::read_craft_facility(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.craft_facility_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<CraftFacilityWriteLease>> NativeCompleteWorldBindingOwner::write_craft_facility(const WorldMoment& at,ExactPhaseWriteContext& writes){
 auto valid=capture(at,writes);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.craft_facility_write(*writable_,*sources_,at,writes);
}
Outcome<std::shared_ptr<const HouseholdServiceReadLease>> NativeCompleteWorldBindingOwner::read_household_service(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.household_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<HouseholdServiceWriteLease>> NativeCompleteWorldBindingOwner::write_household_service(const WorldMoment& at){
 auto valid=mutable_actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.household_write(*writable_,*sources_,at);
}
Outcome<std::shared_ptr<const MotionExactReadLease>> NativeCompleteWorldBindingOwner::read_motion(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.motion_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<MotionExactWriteLease>> NativeCompleteWorldBindingOwner::write_motion(const WorldMoment& at){
 auto valid=mutable_actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.motion_write(*writable_,*sources_,*this,at);
}
Outcome<std::shared_ptr<const StateNeedReadLease>> NativeCompleteWorldBindingOwner::read_state_need(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.state_need_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<StateNeedWriteLease>> NativeCompleteWorldBindingOwner::write_state_need(const WorldMoment& at){
 auto valid=mutable_actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.state_need_write(*writable_,*sources_,at);
}
Outcome<std::shared_ptr<const PoliticalExactReadLease>> NativeCompleteWorldBindingOwner::read_political(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.political_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<PoliticalExactWriteLease>> NativeCompleteWorldBindingOwner::write_political(const WorldMoment& at){
 auto valid=mutable_actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.political_write(*writable_,*sources_,at);
}
Outcome<std::shared_ptr<const TaskWorkReadLease>> NativeCompleteWorldBindingOwner::read_task_work(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.task_work_read(world_,*sources_,*this,at);
}
Outcome<std::shared_ptr<TaskWorkWriteLease>> NativeCompleteWorldBindingOwner::write_task_work(const WorldMoment& at){
 auto valid=mutable_actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.task_work_write(*writable_,*sources_,*this,at);
}
Outcome<std::shared_ptr<const MilitaryExactReadLease>> NativeCompleteWorldBindingOwner::read_military(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.military_read(world_,*sources_,at);
}
Outcome<std::shared_ptr<MilitaryExactWriteLease>> NativeCompleteWorldBindingOwner::write_military(const WorldMoment& at,ExactPhaseWriteContext& writes){
 auto valid=capture(at,writes);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.military_write(*writable_,*sources_,at,writes);
}
Outcome<std::shared_ptr<const DirectorExactReadLease>> NativeCompleteWorldBindingOwner::read_director(const WorldMoment& at)const{
 auto valid=actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.director_read(world_,*sources_,*this,at);
}
Outcome<std::shared_ptr<DirectorExactWriteLease>> NativeCompleteWorldBindingOwner::write_director(const WorldMoment& at){
 auto valid=mutable_actual(at);if(!valid.ok)return {false,{},std::move(valid.error)};
 return factories_.director_write(*writable_,*sources_,*this,at);
}
GrowthCapabilityRevisionWrite* NativeCompleteWorldBindingOwner::growth_capability_writer(){return writer_exports().growth_capability;}
WorkCompletedResultConsumer* NativeCompleteWorldBindingOwner::work_completed_growth_consumer(){return writer_exports().work_completed_growth;}
CareLifeConditionWrite* NativeCompleteWorldBindingOwner::care_life_writer(){return writer_exports().care_life;}
GrowthStartedParticipationWrite* NativeCompleteWorldBindingOwner::growth_start_participation_writer(){return writer_exports().growth_start;}
OriginalGrowthServiceRelationWrite* NativeCompleteWorldBindingOwner::original_growth_service_relation_writer(){return writer_exports().social_growth;}
OriginalTaskReportRelationWrite* NativeCompleteWorldBindingOwner::original_task_report_relation_writer(){return writer_exports().social_task_report;}
OriginalSocialCalendarRelationWrite* NativeCompleteWorldBindingOwner::original_social_calendar_relation_writer(){return writer_exports().social_calendar;}
Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>> WorldBundle::complete_exact_time_bindings()const{return NativeCompleteWorldBindingOwner::create(*this,native_complete_world_factory_registry());}
Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>> WorldBundle::complete_exact_time_bindings(){return NativeCompleteWorldBindingOwner::create(*this,native_complete_world_factory_registry());}
}
