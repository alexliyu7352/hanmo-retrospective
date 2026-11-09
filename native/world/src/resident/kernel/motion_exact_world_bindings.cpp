#include "native_transport_catalog.hpp"
#include "motion_exact_world_bindings.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "exact_complete_world.hpp"
#include "native_transport_funding.hpp"
#include "resident/domains/growth/growth_capability_chain.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/travel/travel_action_time_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
[[noreturn]]void provider_required(const char* code){throw Failure{common_failure(code,{},ErrorKind::unsupported_capability).error};}
Integer version(const CatalogIdentity& identity,const StableId& name){Integer out=0;bool seen=false;for(const auto& item:identity.module_versions)if(item.first==name){if(seen)fail("DuplicateMotionModuleSelection",name);seen=true;if(item.second=="4")out=4;else if(item.second=="5")out=5;else if(item.second=="6")out=6;else if(item.second=="2")out=2;else if(item.second=="1")out=1;else fail("UnsupportedMotionModuleVersion",name);}return out;}
MotionExactAuthorSelection selection(const WorldBundle& w){if(!w.catalog)provider_required("MotionCatalogRequired");return {version(w.catalog->identity,"travel"),version(w.catalog->identity,"group_transport"),version(w.catalog->identity,"transport_manifest_bindings"),version(w.catalog->identity,"cargo_handling"),version(w.catalog->identity,"patrol_checkpoint_sources"),version(w.catalog->identity,"private_transport_funding")};}
void roots(const WorldBundle& w){take(validate_native_transport_layout(w));if(!w.catalog||!w.catalog->travel||!w.catalog->actor_foundation||!w.actor_foundation||!w.movement_exact||!w.spatial_carriers||!w.group_movement_executions||!w.transport_registrations||!w.economic_transport_expenses)provider_required("ExactMotionSelectedRootsRequired");}
// 普通个人旅行也借同一Party接受历史；没有该真实选择时不制造零费用证明。
std::unique_ptr<NativePartyPersonalMovementCostHistoryRead> original_party_history(const WorldBundle& w){
 if(take(selected_native_actor_social_occupancy_version(w.catalog->identity))!=2)return {};
 if(!w.catalog->interaction||!w.services.actor_social_occupancy_world())
  throw Failure{common_failure("MotionOriginalPartyHistoryRequired",{},ErrorKind::unsupported_capability).error};
 return std::make_unique<NativePartyPersonalMovementCostHistoryRead>(w.common,*w.movement_exact,w.interaction,*w.catalog->interaction,w.services.actor_social_occupancy_world()->read());
}
// 编译factory没有捕获；实际资金reader的引用寿命仅本次同world lease。
std::unique_ptr<NativeWorldPersonalTransportFundingRead> original_personal_funding(const WorldBundle& w){
 if(selection(w).private_transport_funding_schema==0){
  take(validate_native_private_transport_funding_layout(w));return {};
 }
 return take(NativeWorldPersonalTransportFundingRead::create(w));
}
class Read final:public MotionExactReadLease {
 const WorldBundle& w_;std::unique_ptr<NativeWorldPersonalTransportFundingRead> funding_;std::unique_ptr<NativePartyPersonalMovementCostHistoryRead> party_;std::shared_ptr<const MotionGroupReadDependencies> group_;std::shared_ptr<const MotionCargoReadDependencies> cargo_;std::shared_ptr<const PatrolCheckpointOriginalRead> patrol_;ActionElapsedStore unselected_empty_elapsed_;NativePersonalMotionContactRead personal_;
public:
 Read(const WorldBundle& w,std::shared_ptr<const MotionGroupReadDependencies> group,std::shared_ptr<const MotionCargoReadDependencies> cargo,std::shared_ptr<const PatrolCheckpointOriginalRead> patrol):w_(w),funding_(original_personal_funding(w)),party_(funding_?nullptr:original_party_history(w)),group_(std::move(group)),cargo_(std::move(cargo)),patrol_(std::move(patrol)),personal_(w.common,*w.movement_exact,w.interaction){}
 MotionExactReadBinding bindings()const override{return {w_.common,w_.travel,*w_.movement_exact,*w_.spatial_carriers,*w_.group_movement_executions,*w_.economic_transport_expenses,*w_.catalog->travel,w_.services,group_?&group_->group_context():nullptr,*w_.actor_foundation,*w_.catalog->actor_foundation,group_?group_->contact_sources():static_cast<const MotionContactBusinessRead&>(personal_),selection(w_),w_.action_elapsed?&*w_.action_elapsed:&unselected_empty_elapsed_,cargo_?&cargo_->cargo_context():nullptr,patrol_.get(),funding_?&funding_->funding():nullptr,funding_?&funding_->party_history():party_.get()};}
};
class Write final:public MotionExactWriteLease {
 WorldBundle& w_;WorldCurrentMomentRead clock_;
 std::unique_ptr<NativeWorldPersonalTransportFundingRead> funding_;
 // 只持本次写 lease 的真实拥有根；不能让 phase4 借空指针漏掉逐 actor 能力应用。
 std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
 CompleteExactWorldBindingOwner* actual_complete_;
 std::optional<CharacterConditionSources> body_;
 std::shared_ptr<MotionGroupWriteDependencies> group_;std::shared_ptr<MotionCargoWriteDependencies> cargo_;std::shared_ptr<const PatrolCheckpointOriginalRead> patrol_;ActionElapsedStore unselected_empty_elapsed_;NativePersonalMotionContactSources personal_;
public:
 Write(WorldBundle& w,std::shared_ptr<MotionGroupWriteDependencies> group,std::shared_ptr<MotionCargoWriteDependencies> cargo,std::shared_ptr<const PatrolCheckpointOriginalRead> patrol,std::unique_ptr<CompleteExactWorldBindingOwner> complete,CompleteExactWorldBindingOwner* borrowed=nullptr):w_(w),clock_(w),funding_(original_personal_funding(w)),complete_(std::move(complete)),actual_complete_(complete_?complete_.get():borrowed),group_(std::move(group)),cargo_(std::move(cargo)),patrol_(std::move(patrol)),personal_(w.common,w.travel,*w.movement_exact,w.interaction,*w.catalog->travel,w.services,w.services.player_actor_id(),{},[this](const StableId& id){return release_action(w_,id);}){}
 MotionExactWriteBinding bindings()override{
  GrowthCapabilityRevisionWrite* capability=nullptr;
  if(actual_complete_){
   if(!w_.catalog->growth)provider_required("MotionActualGrowthCatalogRequired");
   body_.emplace(take(actual_complete_->reads().character_conditions.sources()));
   const auto at=take(w_.actual_phase_moment());
   if(&body_->common!=&w_.common||&body_->state!=&*w_.actor_foundation||
      &body_->catalog!=w_.catalog->actor_foundation.get()||body_->movement!=&*w_.movement_exact||
      body_->growth!=&w_.growth||body_->growth_catalog!=w_.catalog->growth.get()||
      take(compare_world_moments(body_->actual_now,at)))throw Failure{common_failure("MotionSameCandidateConditionSourcesRequired",{},ErrorKind::internal).error};
   capability=actual_complete_->growth_capability_writer();
   if(w_.catalog->growth->complete_capability_application_chain&&
      (!w_.growth_permanent||!capability||body_->permanent_growth!=&*w_.growth_permanent||
       !capability->bound_to(w_.common,*w_.growth_permanent)))
      throw Failure{common_failure("MotionActualCapabilityWriterRequired",{},ErrorKind::unsupported_capability).error};
  }
  return {w_.common,w_.travel,*w_.movement_exact,*w_.spatial_carriers,*w_.group_movement_executions,w_.local_economy,*w_.economic_transport_expenses,*w_.catalog->travel,w_.services,clock_,group_?&group_->group_context():nullptr,*w_.actor_foundation,*w_.catalog->actor_foundation,group_?group_->contact_sources():static_cast<MotionContactBusinessSources&>(personal_),[this](const StableId& id){return release_action(w_,id);},w_.action_elapsed?&*w_.action_elapsed:&unselected_empty_elapsed_,cargo_?&cargo_->cargo_context():nullptr,patrol_.get(),[this](ExactPhaseWriteContext& context){return consume_complete_growth_spatial_inputs_in_candidate(w_,context);},body_?&*body_:nullptr,capability,funding_?&funding_->funding():nullptr};}
};
void validate_native_motion_write_dependencies(const WorldBundle& w,const std::shared_ptr<MotionGroupWriteDependencies>& group,const std::shared_ptr<MotionCargoWriteDependencies>& cargo,const std::shared_ptr<const PatrolCheckpointOriginalRead>& patrol){roots(w);auto selected=selection(w);if(selected.travel_schema!=4&&selected.travel_schema!=5)provider_required("ExactMotionAuthorSelectionMismatch");if(selected.group_schema!=0&&!group)provider_required("GroupMotionProvidersRequired");if((selected.patrol_checkpoint_schema==2)!=bool(patrol))provider_required("PatrolOriginalProviderRequired");if((selected.cargo_handling_schema==1)!=bool(cargo))provider_required("CargoHandlingWorldProvidersRequired");if(cargo){if(!w.world_timeline||!w.catalog->travel)provider_required("CargoHandlingClockRequired");for(const auto& row:w.catalog->travel->cargo_handling_rules.rows())for(auto mode:row.second.labor_modes)if(mode==CargoHandlingLaborMode::named_workers&&!w.action_elapsed)provider_required("CargoHandlingNamedElapsedRequired");}}
Outcome<std::shared_ptr<const MotionExactReadLease>> personal_read(const WorldBundle& w){return make_native_motion_read_lease(w);}
Outcome<std::shared_ptr<MotionExactWriteLease>> personal_write(WorldBundle& w){return make_native_motion_write_lease(w);}
}
Outcome<std::shared_ptr<const MotionExactReadLease>> make_native_motion_read_lease(const WorldBundle& w,std::shared_ptr<const MotionGroupReadDependencies> group,std::shared_ptr<const MotionCargoReadDependencies> cargo,std::shared_ptr<const PatrolCheckpointOriginalRead> patrol){return guard<std::shared_ptr<const MotionExactReadLease>>([&]{roots(w);if(cargo){if(!w.world_timeline)provider_required("CargoHandlingClockRequired");for(const auto& row:w.catalog->travel->cargo_handling_rules.rows())for(auto mode:row.second.labor_modes)if(mode==CargoHandlingLaborMode::named_workers&&!w.action_elapsed)provider_required("CargoHandlingNamedElapsedRequired");}auto lease=std::make_shared<Read>(w,std::move(group),std::move(cargo),std::move(patrol));take(validate_motion_exact_author_selection(lease->bindings()));return std::shared_ptr<const MotionExactReadLease>(std::move(lease));});}
Outcome<std::shared_ptr<MotionExactWriteLease>> make_native_motion_write_lease(WorldBundle& w,std::shared_ptr<MotionGroupWriteDependencies> group,std::shared_ptr<MotionCargoWriteDependencies> cargo,std::shared_ptr<const PatrolCheckpointOriginalRead> patrol){return guard<std::shared_ptr<MotionExactWriteLease>>([&]{validate_native_motion_write_dependencies(w,group,cargo,patrol);std::unique_ptr<CompleteExactWorldBindingOwner> complete;
 if(w.catalog->exact_time&&exact_complete_selected(*w.catalog->exact_time)){
  complete=take(w.complete_exact_time_bindings());
  if(!complete)throw Failure{common_failure("MotionCompleteOwningSourcesRequired",{},ErrorKind::unsupported_capability).error};
 }
 auto result=std::make_shared<Write>(w,std::move(group),std::move(cargo),std::move(patrol),std::move(complete));
 // 在原运动 writer 前验证实际借源及选择，不允许写完用收口扫描补序。
 (void)result->bindings();
 return std::shared_ptr<MotionExactWriteLease>(std::move(result));});}
Outcome<std::shared_ptr<MotionExactWriteLease>> make_native_motion_write_lease_in_owner(
 WorldBundle& w,CompleteExactWorldBindingOwner& owner,
 std::shared_ptr<MotionGroupWriteDependencies> group,
 std::shared_ptr<MotionCargoWriteDependencies> cargo,
 std::shared_ptr<const PatrolCheckpointOriginalRead> patrol){return guard<std::shared_ptr<MotionExactWriteLease>>([&]{
 validate_native_motion_write_dependencies(w,group,cargo,patrol);
 if(!w.catalog->exact_time||!exact_complete_selected(*w.catalog->exact_time)||
    !w.catalog->complete_exact_time_runtime||
    &owner.reads().runtime!=w.catalog->complete_exact_time_runtime.get())
  provider_required("MotionCompleteRetainedOwnerRequired");
 auto result=std::make_shared<Write>(w,std::move(group),std::move(cargo),std::move(patrol),
                                   std::unique_ptr<CompleteExactWorldBindingOwner>{},&owner);
 (void)result->bindings();
 return std::shared_ptr<MotionExactWriteLease>(std::move(result));
});}
MotionExactWorldFactory native_personal_motion_world_factory(){return {&personal_read,&personal_write};}
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_movement_action_time_reader(const WorldBundle& w){return guard<std::shared_ptr<const ActionTimeOriginalSourceRead>>([&]{if(!w.catalog||!w.catalog->travel||!w.movement_exact||(version(w.catalog->identity,"travel")!=4&&version(w.catalog->identity,"travel")!=5))provider_required("ExactMovementActionTimeSourcesRequired");return std::shared_ptr<const ActionTimeOriginalSourceRead>(std::make_shared<NativeMovementActionTimeRead>(w.common,w.travel,*w.movement_exact,*w.catalog->travel,w.services));});}
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_cargo_handling_action_time_reader(const WorldBundle& w,std::shared_ptr<const MotionCargoReadDependencies> dependencies){return guard<std::shared_ptr<const ActionTimeOriginalSourceRead>>([&]{if(!dependencies||!w.movement_exact||!w.action_elapsed||!w.catalog||version(w.catalog->identity,"cargo_handling")!=1)provider_required("CargoHandlingActionTimeProvidersRequired");struct Owner {std::shared_ptr<const MotionCargoReadDependencies> dependencies;NativeCargoHandlingActionTimeRead reader;Owner(const WorldBundle& world,std::shared_ptr<const MotionCargoReadDependencies> d):dependencies(std::move(d)),reader(world.common,world.movement_exact->cargo_handling,*world.action_elapsed,dependencies->cargo_context()){} };auto owner=std::make_shared<Owner>(w,std::move(dependencies));const auto* reader=&owner->reader;return std::shared_ptr<const ActionTimeOriginalSourceRead>(std::move(owner),reader);});}
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_travel_action_time_reader(const WorldBundle& w,std::shared_ptr<const MotionCargoReadDependencies> cargo){return guard<std::shared_ptr<const ActionTimeOriginalSourceRead>>([&]{auto movement=take(make_native_movement_action_time_reader(w));const bool selected=version(w.catalog->identity,"cargo_handling")==1;if(selected!=bool(cargo))provider_required("CargoHandlingActionTimeSelectionMismatch");std::shared_ptr<const ActionTimeOriginalSourceRead> handling;if(cargo)handling=take(make_native_cargo_handling_action_time_reader(w,std::move(cargo)));return std::shared_ptr<const ActionTimeOriginalSourceRead>(std::make_shared<NativeTravelActionTimeRead>(std::move(movement),std::move(handling)));});}

}
