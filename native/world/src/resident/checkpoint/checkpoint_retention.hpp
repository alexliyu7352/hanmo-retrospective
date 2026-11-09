// 内存 census 已退役；保留旧调用点的同 job 归属/取消/首错接头，不存字节账或字段图。
#pragma once
#include "checkpoint_job_memory.hpp"
#include <set>
#include <string>
namespace hanmo::resident {
class CheckpointRetentionCensus {
public:
  CheckpointRetentionCensus(std::shared_ptr<CheckpointMemoryResource>,CheckpointJobUse);
  CheckpointRetentionCensus(const CheckpointRetentionCensus&)=delete;
  CheckpointRetentionCensus& operator=(const CheckpointRetentionCensus&)=delete;
  void owner(const std::shared_ptr<CheckpointAllocationOwner>&);
  void object(const CheckpointObjectAllocation&);
  void resource(const std::shared_ptr<CheckpointMemoryResource>&);
  bool visit_shared_object(const void*);
  template<class T,class A>void vector(const std::vector<T,A>&) noexcept {}
  template<class T>void vector(const std::vector<T,CheckpointAllocator<T>>& value){owner(value.get_allocator().allocation_owner());}
  bool accounts(const std::shared_ptr<CheckpointMemoryResource>& value)const noexcept{return job_resource_&&value&&job_resource_->shares_job(*value);}
  bool accounts(const CheckpointJobReservation& value)const noexcept{return checkpoint_resource_shares_job(job_resource_,value);}
  const std::shared_ptr<CheckpointMemoryResource>& scratch_resource()const noexcept{return job_resource_;}
  void original_token(const CheckpointJobReservation&);
private:
  std::shared_ptr<CheckpointMemoryResource> job_resource_;
  CheckpointJobUse retained_use_;
  std::set<const void*,std::less<const void*>,CheckpointAllocator<const void*>> visited_;
};
}
