#include "checkpoint_retention.hpp"
namespace hanmo::resident {
CheckpointRetentionCensus::CheckpointRetentionCensus(std::shared_ptr<CheckpointMemoryResource> resource,CheckpointJobUse use)
  :job_resource_(resource),retained_use_(use),visited_(std::less<const void*>{},CheckpointAllocator<const void*>{std::move(resource)}){}
bool CheckpointRetentionCensus::visit_shared_object(const void* value){return value&&visited_.insert(value).second;}
void CheckpointRetentionCensus::resource(const std::shared_ptr<CheckpointMemoryResource>& value){
  if(!value)return;
  if(accounts(value)){
    auto checked=checkpoint_resource_add_constraints(value,retained_use_);
    if(!checked.ok)throw checkpoint_job_detail::FailureException{std::move(checked.error)};
  }else if(visit_shared_object(value.get()))value->retain_control(*this);
}
void CheckpointRetentionCensus::owner(const std::shared_ptr<CheckpointAllocationOwner>& value){if(value)resource(value->resource());}
void CheckpointRetentionCensus::object(const CheckpointObjectAllocation& value){resource(value.resource);}
void CheckpointRetentionCensus::original_token(const CheckpointJobReservation& value){
  if(!accounts(value))return;
  auto checked=value.add_constraints(retained_use_);
  if(!checked.ok)throw checkpoint_job_detail::FailureException{std::move(checked.error)};
}
}
