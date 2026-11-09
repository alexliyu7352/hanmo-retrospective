// 经济派生索引只复制具名深字段；真实vector/map分配由CheckpointAllocator收费。
#pragma once
#include "checkpoint_copy_shape.hpp"
#include "resident/domains/local_economy/views/local_economy_view.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement_view.hpp"
#include "resident/domains/local_economy/household/provision_management_view.hpp"
#include <set>
namespace hanmo::resident::index_memory {


template<class T>T copy(CheckpointMemoryCharge& owner,const T& value){(owner).grow(0);return T(value);}
template<class T>void replace(CheckpointMemoryCharge& owner,T& target,const T& source){
    if(&target==&source)return;T next=copy(owner,source);static_assert(std::is_nothrow_move_constructible_v<T>,"index owned transfer");
    target.~T();new(&target)T(std::move(next));
}
template<class T>void clear_values(CheckpointMemoryCharge&,CheckpointVector<T>& rows){rows.clear();}
template<class Map>void clear(CheckpointMemoryCharge&,Map& rows){rows.clear();}
template<class Map>auto& entry(CheckpointMemoryCharge& owner,Map& rows,const StableId& key){auto found=rows.find(key);if(found!=rows.end())return found->second;auto copied=copy(owner,key);return rows.emplace(std::move(copied),typename Map::mapped_type{}).first->second;}
template<class Map,class Value>void insert(CheckpointMemoryCharge& owner,Map& rows,const StableId& key,const Value& value){if(rows.find(key)!=rows.end())return;auto k=copy(owner,key);auto v=copy(owner,value);rows.emplace(std::move(k),std::move(v));}
template<class Map>auto& authority_entry(CheckpointMemoryCharge& owner,Map& rows,const ActingAuthority& a){
    for(auto& row:rows)if(std::get<0>(row.first)==a.organization_id&&std::get<1>(row.first)==a.office_id&&std::get<2>(row.first)==a.authority_epoch&&std::get<3>(row.first)==a.actor_id)return row.second;
    (owner).grow(0);LocalEconomyAuthorityKey key{a.organization_id,a.office_id,a.authority_epoch,a.actor_id};return rows.emplace(std::move(key),typename Map::mapped_type{}).first->second;
}
inline ActingAuthority authority(CheckpointMemoryCharge& owner,const StableId& org,const StableId& office,Integer epoch,const StableId& actor){(owner).grow(0);return {org,office,epoch,actor};}
inline LocalEconomySource source(CheckpointMemoryCharge& owner,LocalEconomySource::Kind kind,const StableId& id){return {kind,copy(owner,id)};}
struct BorrowedIdLess {bool operator()(const StableId* a,const StableId* b)const{return *a<*b;}};
using BorrowedIds=std::set<const StableId*,BorrowedIdLess,CheckpointAllocator<const StableId*>>;
using BorrowedIdMap=std::map<const StableId*,const StableId*,BorrowedIdLess,CheckpointAllocator<std::pair<const StableId* const,const StableId*>>>;
}
