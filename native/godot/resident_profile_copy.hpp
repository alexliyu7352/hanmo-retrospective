// 原 META 的普通深复制、真实容器及同 job 生命周期；不保逐字段计量图。
#pragma once
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/growth/card_acquisition_contract.hpp"
namespace hanmo::resident {class CheckpointJobBudget;class CheckpointRetentionCensus;}
namespace hanmo::resident::storage {
struct AdmissionRecord;struct ProfileMetadataSnapshot;struct SlotAdmissionCapsule;
void retain_profile_metadata(const ProfileMetadataSnapshot&,CheckpointRetentionCensus&);
CheckpointMemoryCharge reserve_profile_metadata_copy(const ProfileMetadataSnapshot&);
// 原容器按实际数量 reserve；先核大小溢出与 job，再执行唯一普通分配。
template<class T>void reserve_profile_rows(std::vector<T>& rows,std::size_t count,CheckpointMemoryCharge& charge){
    if(count<=rows.capacity())return;
    if(count>rows.max_size()||count>SIZE_MAX/sizeof(T))throw std::bad_alloc();
    charge.grow(0);rows.reserve(count);
}
}
