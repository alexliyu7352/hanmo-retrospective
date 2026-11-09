// S0真实写口与提交后投影只借同一WorldBundle；无外部拼装sources或按显示卡补发。
#include "profile_delivery_capture.hpp"
#include "world_impl.hpp"
#include "resident/domains/growth/profile_acquisition_world.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
namespace hanmo::resident {
Outcome<Integer> profile_delivery_selected_version(const CatalogIdentity& identity){
    Integer selected=0;for(const auto& item:identity.module_versions)if(item.first=="profile_deliveries"){
        if(selected||item.second!="1")return {false,{},common_failure("ProfileDeliveryVersionUnsupported").error};selected=1;
    }return {true,selected,{}};
}
Status WorldBundle::record_profile_acquisition(const ProfileAcquisitionSource& source){
    auto selected=profile_delivery_selected_version(catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(selected.value!=1)return common_failure("ProfileDeliveriesNotSelected",{},ErrorKind::unsupported_capability);
    auto read=profile_acquisition_read();if(!read.ok)return {false,{},std::move(read.error)};
    return append_profile_acquisition_deliveries(common,profile_deliveries,read.value->sources(),card_acquisitions,read.value->control_sources(),source);
}
Status WorldBundle::validate_profile_deliveries()const{
    auto selected=profile_delivery_selected_version(catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(!selected.value)return profile_deliveries.entries.empty()?common_success():common_failure("UnexpectedProfileDeliveries");
    auto read=profile_acquisition_read();if(!read.ok)return {false,{},std::move(read.error)};
    return validate_profile_delivery_outbox(common.state(),profile_deliveries,read.value->sources(),read.value->acquisitions(),read.value->control_sources());
}
CheckpointJobResult<CommittedProfileDeliveries> ResidentWorld::capture_profile_deliveries(const CapacityProfile& capacity,CheckpointJobBudget& job)const{
    try{
        auto checked=job.check_profile(capacity);if(!checked.ok)return {false,{},std::move(checked.error)};
        if(!impl_->world)return {false,{},job.capture_boundary_failure("NoWorld",{},ErrorKind::rule)};
        auto selected=profile_delivery_selected_version(impl_->world->catalog->identity);if(!selected.ok)return {false,{},job.capture_failure(std::move(selected.error))};
        if(selected.value!=1)return {false,{},job.capture_boundary_failure("ProfileDeliveriesNotSelected",{},ErrorKind::unsupported_capability)};
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
        // 同所属world线程同步取出，adapter与被借世界都保活至完成；worker只接拥有结果。
        const auto& world=*impl_->world;auto read=world.profile_acquisition_read();if(!read.ok)return {false,{},job.capture_failure(std::move(read.error))};
        return CommittedProfileDeliveries::capture(world.common.state(),world.profile_deliveries,read.value->sources(),read.value->acquisitions(),read.value->control_sources(),capacity,job);
    }catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
}
