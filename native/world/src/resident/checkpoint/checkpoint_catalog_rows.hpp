// Original private author-row builder shared by actual catalog decoders.
#pragma once
#include "resident/storage/common_codec.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <string_view>
namespace hanmo::resident {
// Original private decoder tables accumulate in actual job-allocated rows.
// Lookups and first-duplicate checks keep the original insertion order. Only
// the final original table adoption constructs order/index, with its own fee.
template<class T>class CheckpointCatalogRows {
 using Row=std::pair<StableId,T>;CheckpointVector<Row> rows_;std::string_view path_;
public:
 explicit CheckpointCatalogRows(std::size_t count,std::string_view path):path_(path){rows_.reserve(count);}
 const T* find(std::string_view id)const{for(const auto& row:rows_)if(row.first==id)return &row.second;return nullptr;}
 bool insert(const StableId& id,T value){if(find(id))return false;if(rows_.size()==rows_.capacity())boundary::fail("CheckpointDecodedTransferShapeMismatch",path_);rows_.emplace_back(checkpoint_decode_boundary_string(id),std::move(value));return true;}
 std::size_t size()const noexcept{return rows_.size();}
 void publish(OrderedTable<T>& destination){
  if(!current_checkpoint_memory()){std::vector<Row> ordinary;ordinary.reserve(rows_.size());for(auto& row:rows_)ordinary.emplace_back(std::move(row));if(!destination.adopt_checkpoint_rows(std::move(ordinary)))boundary::fail("DuplicateCheckpointTableKey","checkpoint/table");return;}
  auto status=destination.adopt_checkpoint_rows(std::move(rows_),[](auto& out,Row& row)->Status{out.emplace_back(std::move(row));return {true,{},{}};});if(!status.ok)boundary::fail_parts(status.error.original.code,{status.error.original.path},status.error.kind);}
};
}
