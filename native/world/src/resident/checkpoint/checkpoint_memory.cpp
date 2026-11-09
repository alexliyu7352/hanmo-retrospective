// 仅显式采用的容器获取线程scope；资源生命周期由容器allocator共享拥有。
#include "hanmo/resident/checkpoint_memory.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace hanmo::resident {
namespace {thread_local std::shared_ptr<CheckpointMemoryResource> selected;}
void CheckpointMemoryResource::retain_control(CheckpointRetentionCensus&)const{throw std::logic_error("CheckpointUntrackedMemoryResource");}
std::shared_ptr<CheckpointMemoryResource> current_checkpoint_memory()noexcept{return selected;}
ScopedCheckpointMemory::ScopedCheckpointMemory(std::shared_ptr<CheckpointMemoryResource> resource)noexcept:previous_(std::move(selected)){selected=std::move(resource);}
ScopedCheckpointMemory::~ScopedCheckpointMemory()noexcept{selected=std::move(previous_);}
namespace {
// owner 控制块仍使用同一无递归 allocator；删除只为计量捕获 extent 的 TLS。
template<class T>struct OwnerAllocator {
    using value_type=T;std::shared_ptr<CheckpointMemoryResource> resource;
    explicit OwnerAllocator(std::shared_ptr<CheckpointMemoryResource> r):resource(std::move(r)){}
    template<class U>OwnerAllocator(const OwnerAllocator<U>& x)noexcept:resource(x.resource){}
    T* allocate(std::size_t count){if(count>SIZE_MAX/sizeof(T))throw std::bad_alloc();const auto bytes=count*sizeof(T);T* result=resource?static_cast<T*>(resource->allocate_bytes(bytes,alignof(T))):std::allocator<T>{}.allocate(count);return result;}
    void deallocate(T* value,std::size_t count)noexcept{if(resource)resource->deallocate_bytes(value);else std::allocator<T>{}.deallocate(value,count);}
    template<class U>bool operator==(const OwnerAllocator<U>& x)const noexcept{return resource==x.resource;}
};
}
std::shared_ptr<CheckpointAllocationOwner> checkpoint_allocation_owner(std::shared_ptr<CheckpointMemoryResource> resource){return std::allocate_shared<CheckpointAllocationOwner>(OwnerAllocator<CheckpointAllocationOwner>{resource},resource);}

namespace {struct ObjectHeader {std::shared_ptr<CheckpointMemoryResource> resource;void* original;};}
void* checkpoint_allocate_object(std::size_t bytes,std::size_t alignment){
    alignment=std::max(alignment,alignof(ObjectHeader));if(!alignment||(alignment&(alignment-1))||bytes>SIZE_MAX-sizeof(ObjectHeader)-(alignment-1))throw std::bad_alloc();
    const auto total=bytes+sizeof(ObjectHeader)+alignment-1;auto resource=current_checkpoint_memory();
    void* raw=resource?resource->allocate_bytes(total,alignof(ObjectHeader)) : ::operator new(total);
    const auto start=reinterpret_cast<std::uintptr_t>(raw)+sizeof(ObjectHeader);const auto aligned=(start+alignment-1)&~std::uintptr_t(alignment-1);
    new(reinterpret_cast<ObjectHeader*>(aligned)-1)ObjectHeader{std::move(resource),raw};return reinterpret_cast<void*>(aligned);
}
CheckpointObjectAllocation checkpoint_object_allocation(const void* pointer)noexcept{if(!pointer)return {};const auto* h=reinterpret_cast<const ObjectHeader*>(pointer)-1;return {h->resource};}
void checkpoint_deallocate_object(void* pointer)noexcept{
    if(!pointer)return;auto* header=reinterpret_cast<ObjectHeader*>(pointer)-1;auto resource=std::move(header->resource);void* raw=header->original;header->~ObjectHeader();
    if(resource)resource->deallocate_bytes(raw);else ::operator delete(raw);
}

}
