// Compiled adapters acquire a real domain lease directly from the one complete
// owner. They never call that owner's public factory aggregate recursively.
#include "exact_complete_world.hpp"
#include "world_bundle.hpp"
#include "native_born_population_state.hpp"
#include "resident/domains/craft/craft_exact_phase.hpp"
#include "resident/domains/craft/repair_exact_phase.hpp"
#include "resident/domains/craft/craft_facility_runtime.hpp"
#include "resident/domains/travel/movement_exact_phase.hpp"
#include "resident/domains/affairs/state_need_exact_phase.hpp"
#include "resident/domains/politics/political_exact_phase.hpp"
#include "resident/domains/tasks/work_execution/work_exact_runtime.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace hanmo::resident { namespace {
struct Failure { RuleError error; };
template<class T>T take(Outcome<T> value){
    if(!value.ok)throw Failure{std::move(value.error)};
    return std::move(value.value);
}
[[noreturn]]void fail(const char* code){
    throw Failure{common_failure(code,{},ErrorKind::internal).error};
}
template<class T,class F>Outcome<T> guarded(F&& fn){
    try{return {true,fn(),{}};}
    catch(Failure& error){return {false,{},std::move(error.error)};}
    catch(const std::bad_alloc&){return {false,{},common_failure("NativeDomainLeaseResourceExhausted",{},ErrorKind::internal).error};}
    catch(const std::length_error&){return {false,{},common_failure("NativeDomainLeaseResourceExhausted",{},ErrorKind::internal).error};}
}
void actual_moment(const WorldBundle& world,const WorldMoment& at){
    if(!world.catalog||!world.catalog->exact_time||
       !exact_complete_selected(*world.catalog->exact_time)||
       !world.catalog->complete_exact_time_runtime)
        fail("NativeDomainLeaseCompleteRuntimeRequired");
    if(take(compare_world_moments(take(world.actual_phase_moment()),at)))
        fail("NativeDomainLeaseActualMomentMismatch");
}
void actual_owner(const WorldBundle& world,const CompleteExactWorldBindingOwner* owner){
    if(!owner)fail("NativeDomainLeaseOwnerRequired");
    if(&owner->reads().runtime!=world.catalog->complete_exact_time_runtime.get())
        fail("NativeDomainLeaseRuntimeMismatch");
}

// The member order is the lifetime contract: the inner domain lease and all
// its snapshot contexts are destroyed before the complete dependency owner.
// Domain acquisition must not retain this outer wrapper (no ownership cycle).
template<class Lease,class Binding>
class RetainedReadLease final:public Lease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
    std::shared_ptr<const Lease> inner_;
public:
    RetainedReadLease(std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                      std::shared_ptr<const Lease> inner)
        :owner_(std::move(owner)),inner_(std::move(inner)){}
    Binding bindings()const override{return inner_->bindings();}
};
template<class Lease,class Binding>
class RetainedWriteLease final:public Lease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
    std::shared_ptr<Lease> inner_;
public:
    RetainedWriteLease(std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                       std::shared_ptr<Lease> inner)
        :owner_(std::move(owner)),inner_(std::move(inner)){}
    Binding bindings()override{return inner_->bindings();}
};
class RetainedCraftFacilityReadLease final:public CraftFacilityReadLease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
    std::shared_ptr<const CraftFacilityReadLease> inner_;
public:
    RetainedCraftFacilityReadLease(std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                                  std::shared_ptr<const CraftFacilityReadLease> inner)
        :owner_(std::move(owner)),inner_(std::move(inner)){}
    bool bound_to(const WorldBundle& world)const noexcept override{return inner_->bound_to(world);}
    CraftFacilityReadBinding bindings()const override{return inner_->bindings();}
};
class RetainedCraftFacilityWriteLease final:public CraftFacilityWriteLease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
    std::shared_ptr<CraftFacilityWriteLease> inner_;
public:
    RetainedCraftFacilityWriteLease(std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                                   std::shared_ptr<CraftFacilityWriteLease> inner)
        :owner_(std::move(owner)),inner_(std::move(inner)){}
    bool bound_to(const WorldBundle& world)const noexcept override{return inner_->bound_to(world);}
    CraftFacilityWriteBinding bindings()override{return inner_->bindings();}
};
class RetainedDirectorReadLease final:public DirectorExactReadLease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
    std::shared_ptr<const DirectorExactReadLease> inner_;
public:
    RetainedDirectorReadLease(std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                              std::shared_ptr<const DirectorExactReadLease> inner)
        :owner_(std::move(owner)),inner_(std::move(inner)){}
    DirectorExactReadBinding reads()const override{return inner_->reads();}
};
class RetainedDirectorWriteLease final:public DirectorExactWriteLease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
    std::shared_ptr<DirectorExactWriteLease> inner_;
public:
    RetainedDirectorWriteLease(std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                               std::shared_ptr<DirectorExactWriteLease> inner)
        :owner_(std::move(owner)),inner_(std::move(inner)){}
    DirectorExactWriteBinding writes()override{return inner_->writes();}
};
template<class Lease,class Binding,class Acquire>
Outcome<std::shared_ptr<const Lease>> read_lease(
    const WorldBundle& world,const WorldMoment& at,Acquire&& acquire){
    return guarded<std::shared_ptr<const Lease>>([&]{
        actual_moment(world,at);
        auto owner=take(world.complete_exact_time_bindings());
        actual_owner(world,owner.get());
        auto inner=take(acquire(std::as_const(*owner)));
        if(!inner)fail("NativeDomainReadLeaseRequired");
        using Retained=RetainedReadLease<Lease,Binding>;
        auto retained=checkpoint_share(std::unique_ptr<Retained>(
            new Retained(std::move(owner),std::move(inner))));
        return std::shared_ptr<const Lease>(std::move(retained));
    });
}
template<class Lease,class Binding,class Acquire>
Outcome<std::shared_ptr<Lease>> write_lease(
    WorldBundle& world,const WorldMoment& at,Acquire&& acquire){
    return guarded<std::shared_ptr<Lease>>([&]{
        actual_moment(world,at);
        auto owner=take(world.complete_exact_time_bindings());
        actual_owner(world,owner.get());
        auto inner=take(acquire(*owner));
        if(!inner)fail("NativeDomainWriteLeaseRequired");
        using Retained=RetainedWriteLease<Lease,Binding>;
        auto retained=checkpoint_share(std::unique_ptr<Retained>(
            new Retained(std::move(owner),std::move(inner))));
        return std::shared_ptr<Lease>(std::move(retained));
    });
}
Outcome<std::shared_ptr<const EconomicExactReadLease>> economic_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<EconomicExactReadLease,EconomicExactReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){auto inner=take(owner.read_economic(at));if(!inner)fail("NativeEconomicReadLeaseRequired");auto binding=inner->bindings();take(validate_native_born_population_read_binding(world,binding));return Outcome<std::shared_ptr<const EconomicExactReadLease>>{true,std::move(inner),{}};});
}
Outcome<std::shared_ptr<EconomicExactWriteLease>> economic_write(
    WorldBundle& world,const WorldMoment& at,const DynamicStepInput* phase){
    return write_lease<EconomicExactWriteLease,EconomicExactWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){auto inner=take(owner.write_economic(at,phase));if(!inner)fail("NativeEconomicWriteLeaseRequired");auto binding=inner->bindings();take(validate_native_born_population_write_binding(world,binding));return Outcome<std::shared_ptr<EconomicExactWriteLease>>{true,std::move(inner),{}};});
}
Outcome<std::shared_ptr<const CraftExactReadLease>> craft_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<CraftExactReadLease,CraftExactReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){return owner.read_craft(at);});
}
Outcome<std::shared_ptr<CraftExactWriteLease>> craft_write(
    WorldBundle& world,const WorldMoment& at,ExactPhaseWriteContext& writes){
    return write_lease<CraftExactWriteLease,CraftExactWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){return owner.write_craft(at,writes);});
}
Outcome<std::shared_ptr<const RepairExactReadLease>> repair_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<RepairExactReadLease,RepairExactReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){return owner.read_repair(at);});
}
Outcome<std::shared_ptr<RepairExactWriteLease>> repair_write(
    WorldBundle& world,const WorldMoment& at,ExactPhaseWriteContext& writes){
    return write_lease<RepairExactWriteLease,RepairExactWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){return owner.write_repair(at,writes);});
}
Outcome<std::shared_ptr<const CraftFacilityReadLease>> craft_facility_read(
    const WorldBundle& world,const WorldMoment& at){
    return guarded<std::shared_ptr<const CraftFacilityReadLease>>([&]{
        actual_moment(world,at);
        auto owner=take(world.complete_exact_time_bindings());
        actual_owner(world,owner.get());
        auto inner=take(std::as_const(*owner).read_craft_facility(at));
        if(!inner||!inner->bound_to(world))fail("NativeCraftFacilityReadLeaseMismatch");
        auto retained=checkpoint_share(std::unique_ptr<RetainedCraftFacilityReadLease>(
            new RetainedCraftFacilityReadLease(std::move(owner),std::move(inner))));
        return std::shared_ptr<const CraftFacilityReadLease>(std::move(retained));
    });
}
Outcome<std::shared_ptr<CraftFacilityWriteLease>> craft_facility_write(
    WorldBundle& world,const WorldMoment& at,ExactPhaseWriteContext& writes){
    return guarded<std::shared_ptr<CraftFacilityWriteLease>>([&]{
        actual_moment(world,at);
        auto owner=take(world.complete_exact_time_bindings());
        actual_owner(world,owner.get());
        auto inner=take(owner->write_craft_facility(at,writes));
        if(!inner||!inner->bound_to(world))fail("NativeCraftFacilityWriteLeaseMismatch");
        auto retained=checkpoint_share(std::unique_ptr<RetainedCraftFacilityWriteLease>(
            new RetainedCraftFacilityWriteLease(std::move(owner),std::move(inner))));
        return std::shared_ptr<CraftFacilityWriteLease>(std::move(retained));
    });
}
Outcome<std::shared_ptr<const HouseholdServiceReadLease>> household_service_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<HouseholdServiceReadLease,HouseholdServiceReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){return owner.read_household_service(at);});
}
Outcome<std::shared_ptr<HouseholdServiceWriteLease>> household_service_write(
    WorldBundle& world,const WorldMoment& at){
    return write_lease<HouseholdServiceWriteLease,HouseholdServiceWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){return owner.write_household_service(at);});
}
Outcome<std::shared_ptr<const MotionExactReadLease>> motion_read(const WorldBundle& world){
    return guarded<std::shared_ptr<const MotionExactReadLease>>([&]{
        const auto at=take(world.actual_phase_moment());
        return take(read_lease<MotionExactReadLease,MotionExactReadBinding>(world,at,
            [&](const CompleteExactWorldBindingOwner& owner){return owner.read_motion(at);}));
    });
}
Outcome<std::shared_ptr<MotionExactWriteLease>> motion_write(WorldBundle& world){
    return guarded<std::shared_ptr<MotionExactWriteLease>>([&]{
        const auto at=take(world.actual_phase_moment());
        return take(write_lease<MotionExactWriteLease,MotionExactWriteBinding>(world,at,
            [&](CompleteExactWorldBindingOwner& owner){return owner.write_motion(at);}));
    });
}
Outcome<std::shared_ptr<const StateNeedReadLease>> state_need_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<StateNeedReadLease,StateNeedReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){return owner.read_state_need(at);});
}
Outcome<std::shared_ptr<StateNeedWriteLease>> state_need_write(
    WorldBundle& world,const WorldMoment& at){
    return write_lease<StateNeedWriteLease,StateNeedWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){return owner.write_state_need(at);});
}
Outcome<std::shared_ptr<const PoliticalExactReadLease>> political_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<PoliticalExactReadLease,PoliticalExactReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){return owner.read_political(at);});
}
Outcome<std::shared_ptr<PoliticalExactWriteLease>> political_write(
    WorldBundle& world,const WorldMoment& at){
    return write_lease<PoliticalExactWriteLease,PoliticalExactWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){return owner.write_political(at);});
}
Outcome<std::shared_ptr<const TaskWorkReadLease>> task_work_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<TaskWorkReadLease,TaskWorkReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){return owner.read_task_work(at);});
}
Outcome<std::shared_ptr<TaskWorkWriteLease>> task_work_write(
    WorldBundle& world,const WorldMoment& at){
    return write_lease<TaskWorkWriteLease,TaskWorkWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){return owner.write_task_work(at);});
}
Outcome<std::shared_ptr<const MilitaryExactReadLease>> military_read(
    const WorldBundle& world,const WorldMoment& at){
    return read_lease<MilitaryExactReadLease,MilitaryExactReadBinding>(world,at,
        [&](const CompleteExactWorldBindingOwner& owner){return owner.read_military(at);});
}
Outcome<std::shared_ptr<MilitaryExactWriteLease>> military_write(
    WorldBundle& world,const WorldMoment& at,ExactPhaseWriteContext& writes){
    return write_lease<MilitaryExactWriteLease,MilitaryExactWriteBinding>(world,at,
        [&](CompleteExactWorldBindingOwner& owner){return owner.write_military(at,writes);});
}
Outcome<std::shared_ptr<const DirectorExactReadLease>> director_read(
    const WorldBundle& world,const WorldMoment& at){
    return guarded<std::shared_ptr<const DirectorExactReadLease>>([&]{
        actual_moment(world,at);
        auto owner=take(world.complete_exact_time_bindings());
        actual_owner(world,owner.get());
        auto inner=take(std::as_const(*owner).read_director(at));
        if(!inner)fail("NativeDirectorReadLeaseRequired");
        auto retained=checkpoint_share(std::unique_ptr<RetainedDirectorReadLease>(
            new RetainedDirectorReadLease(std::move(owner),std::move(inner))));
        return std::shared_ptr<const DirectorExactReadLease>(std::move(retained));
    });
}
Outcome<std::shared_ptr<DirectorExactWriteLease>> director_write(
    WorldBundle& world,const WorldMoment& at){
    return guarded<std::shared_ptr<DirectorExactWriteLease>>([&]{
        actual_moment(world,at);
        auto owner=take(world.complete_exact_time_bindings());
        actual_owner(world,owner.get());
        auto inner=take(owner->write_director(at));
        if(!inner)fail("NativeDirectorWriteLeaseRequired");
        auto retained=checkpoint_share(std::unique_ptr<RetainedDirectorWriteLease>(
            new RetainedDirectorWriteLease(std::move(owner),std::move(inner))));
        return std::shared_ptr<DirectorExactWriteLease>(std::move(retained));
    });
}
}
EconomicExactWorldFactory native_economic_exact_world_factory()noexcept{
    return {&economic_read,&economic_write};
}
CraftExactWorldFactory native_craft_exact_world_factory()noexcept{
    return {&craft_read,&craft_write};
}
RepairExactWorldFactory native_repair_exact_world_factory()noexcept{
    return {&repair_read,&repair_write};
}
CraftFacilityWorldFactory native_craft_facility_world_factory()noexcept{
    return {&craft_facility_read,&craft_facility_write};
}
HouseholdServiceWorldFactory native_household_service_world_factory()noexcept{
    return {&household_service_read,&household_service_write};
}
MotionExactWorldFactory native_motion_exact_world_factory()noexcept{
    return {&motion_read,&motion_write};
}
StateNeedWorldFactory native_state_need_world_factory()noexcept{
    return {&state_need_read,&state_need_write};
}
PoliticalExactWorldFactory native_political_exact_world_factory()noexcept{
    return {&political_read,&political_write};
}
TaskWorkWorldFactory native_task_work_world_factory()noexcept{
    return {&task_work_read,&task_work_write};
}
MilitaryExactWorldFactory native_military_exact_world_factory()noexcept{
    return {&military_read,&military_write};
}
DirectorExactWorldFactory native_director_exact_world_factory()noexcept{
    return {&director_read,&director_write};
}
}
