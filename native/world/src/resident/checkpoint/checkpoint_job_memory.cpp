// 真实分配头只保原地址；分配前保同 job 的取消／首错门，不维护内存用量账。
#include "checkpoint_job_memory.hpp"
#include "checkpoint_retention.hpp"
#include <mutex>
#include <algorithm>
#include <new>
#include <limits>
#include <cstdint>
namespace hanmo::resident {
namespace {
class JobMemoryResource final:public CheckpointMemoryResource {
    struct Header {void* original;};
    CheckpointJobReservation blocks_;
    CheckpointJobUse use_;
    mutable std::mutex mutex_;
    const CheckpointJobReservation* control_reservation_=nullptr;
public:
    explicit JobMemoryResource(CheckpointJobReservation&& blocks,CheckpointJobUse use)noexcept:blocks_(std::move(blocks)),use_(use){}
    bool memory_accounting_enabled()const noexcept override{return blocks_.memory_accounting_enabled();}
    bool shares_reservation(const CheckpointJobReservation& reservation)const noexcept{return blocks_.shares_job(reservation);}
    bool accounts_candidate(const CheckpointJobReservation& reservation)const noexcept{std::lock_guard<std::mutex> lock(mutex_);return (std::uint8_t(use_)&std::uint8_t(CheckpointJobUse::candidate))&&blocks_.shares_job(reservation);}
    void created_control(const CheckpointJobReservation* token)noexcept{control_reservation_=token;}
    CheckpointJobStatus add_constraints(CheckpointJobUse use){
        std::lock_guard<std::mutex> lock(mutex_);
        if(!control_reservation_)return {false,{},CheckpointJobFailure::fixed(CheckpointJobFailureCode::reservation_mismatch)};
        auto control=control_reservation_->add_constraints(use);if(!control.ok)return control;
        auto blocks=blocks_.add_constraints(use);if(!blocks.ok)return blocks;
        use_=CheckpointJobUse(std::uint8_t(use_)|std::uint8_t(use));return {true,{},{}};
    }
    void retain_control(CheckpointRetentionCensus& c)const override{blocks_.retain_control(c);}
    void reserve_external(std::size_t)override{std::lock_guard<std::mutex> lock(mutex_);auto held=blocks_.grow(0);if(!held.ok)throw checkpoint_job_detail::FailureException{std::move(held.error)};}
    void release_external(std::size_t)noexcept override{}
    bool shares_job(const CheckpointMemoryResource& other)const noexcept override{auto* value=dynamic_cast<const JobMemoryResource*>(&other);return value&&blocks_.shares_job(value->blocks_);}
    void* allocate_bytes(std::size_t size,std::size_t alignment)override{
        alignment=std::max(alignment,alignof(Header));if(!alignment||(alignment&(alignment-1))||size>SIZE_MAX-sizeof(Header)-(alignment-1))throw std::bad_alloc();
        const auto total=size+sizeof(Header)+alignment-1;std::lock_guard<std::mutex> lock(mutex_);auto held=blocks_.grow(0);if(!held.ok)throw checkpoint_job_detail::FailureException{std::move(held.error)};
        void* raw=::operator new(total);
        const auto start=reinterpret_cast<std::uintptr_t>(raw)+sizeof(Header);const auto aligned=(start+alignment-1)&~std::uintptr_t(alignment-1);auto* header=reinterpret_cast<Header*>(aligned)-1;new(header)Header{raw};return reinterpret_cast<void*>(aligned);
    }
    void deallocate_bytes(void* pointer)noexcept override{
        if(!pointer)return;auto* header=reinterpret_cast<Header*>(pointer)-1;void* original=header->original;header->~Header();::operator delete(original);
    }
};
// resource 自身的 combined shared 分配仍有真 token，不估算或登记控制块内存。
// 无递归allocator只在同步factory的TLS窗取得job；控制块仅保存空allocator，释放从自己的头取租约。
struct ResourceExtent {const CheckpointJobReservation* reservation=nullptr;};
thread_local ResourceExtent* creating_resource_extent=nullptr;
thread_local CheckpointJobBudget* creating_resource_job=nullptr;
thread_local CheckpointJobUse creating_resource_use=CheckpointJobUse::slot;
struct ResourceCreationScope {ResourceExtent* previous_extent;CheckpointJobBudget* previous;CheckpointJobUse previous_use;ResourceCreationScope(CheckpointJobBudget& job,CheckpointJobUse use,ResourceExtent& extent):previous_extent(creating_resource_extent),previous(creating_resource_job),previous_use(creating_resource_use){creating_resource_extent=&extent;creating_resource_job=&job;creating_resource_use=use;}~ResourceCreationScope(){creating_resource_extent=previous_extent;creating_resource_job=previous;creating_resource_use=previous_use;}};
struct ResourceAllocationHeader {CheckpointJobReservation reservation;void* original;};
template<class T>struct ResourceAllocator {
    using value_type=T;ResourceAllocator()=default;template<class U>ResourceAllocator(const ResourceAllocator<U>&)noexcept{}
    T* allocate(std::size_t count){
        if(!creating_resource_job||count>SIZE_MAX/sizeof(T))throw std::bad_alloc();const auto alignment=std::max(alignof(T),alignof(ResourceAllocationHeader));const auto bytes=count*sizeof(T);
        if(bytes>SIZE_MAX-sizeof(ResourceAllocationHeader)-(alignment-1))throw std::bad_alloc();const auto total=bytes+sizeof(ResourceAllocationHeader)+alignment-1;
        auto reserved=creating_resource_job->reserve(creating_resource_use,0);if(!reserved.ok)throw checkpoint_job_detail::FailureException{std::move(reserved.error)};
        void* raw=::operator new(total);const auto start=reinterpret_cast<std::uintptr_t>(raw)+sizeof(ResourceAllocationHeader);const auto aligned=(start+alignment-1)&~std::uintptr_t(alignment-1);
        auto* header=new(reinterpret_cast<ResourceAllocationHeader*>(aligned)-1)ResourceAllocationHeader{std::move(reserved.value),raw};if(creating_resource_extent)*creating_resource_extent={&header->reservation};return reinterpret_cast<T*>(aligned);
    }
    void deallocate(T* value,std::size_t)noexcept{auto* header=reinterpret_cast<ResourceAllocationHeader*>(value)-1;void* raw=header->original;auto reserved=std::move(header->reservation);header->~ResourceAllocationHeader();::operator delete(raw);}
    template<class U>bool operator==(const ResourceAllocator<U>&)const noexcept{return true;}
};
}
bool checkpoint_resource_shares_job(const std::shared_ptr<CheckpointMemoryResource>& resource,const CheckpointJobReservation& reservation)noexcept{
    const auto* owned=dynamic_cast<const JobMemoryResource*>(resource.get());return owned&&owned->shares_reservation(reservation);
}
bool checkpoint_resource_accounts_candidate(const std::shared_ptr<CheckpointMemoryResource>& resource,const CheckpointJobReservation& reservation)noexcept{
    const auto* owned=dynamic_cast<const JobMemoryResource*>(resource.get());return owned&&owned->accounts_candidate(reservation);
}
CheckpointJobStatus checkpoint_resource_add_constraints(const std::shared_ptr<CheckpointMemoryResource>& resource,CheckpointJobUse use){
    auto* owned=dynamic_cast<JobMemoryResource*>(resource.get());
    if(!owned)return {false,{},CheckpointJobFailure::fixed(CheckpointJobFailureCode::reservation_mismatch)};
    return owned->add_constraints(use);
}
CheckpointJobResult<std::shared_ptr<CheckpointMemoryResource>> checkpoint_memory_resource(CheckpointJobBudget& job,CheckpointJobUse use){
    auto blocks=job.reserve(use,0);if(!blocks.ok)return {false,{},std::move(blocks.error)};
    try{ResourceExtent extent;ResourceCreationScope window(job,use,extent);auto concrete=std::allocate_shared<JobMemoryResource>(ResourceAllocator<JobMemoryResource>{},std::move(blocks.value),use);concrete->created_control(extent.reservation);std::shared_ptr<CheckpointMemoryResource> resource=std::move(concrete);return {true,std::move(resource),{}};}
    catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
}
