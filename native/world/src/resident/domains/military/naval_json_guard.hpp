// Original Naval2 recursive duplicate-key gate. Same LIFO traversal and error
// path; borrowed comparisons replace the uncharged duplicate-key set.
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include <charconv>
namespace hanmo::resident {
inline Status validate_naval_json_unique_keys(const BoundaryValue& root,const StableId& path){
 CheckpointVector<std::pair<const BoundaryValue*,StableId>> pending;
 pending.emplace_back(&root,checkpoint_decode_boundary_string(path));
 while(!pending.empty()){
  auto current=std::move(pending.back());pending.pop_back();
  if(const auto* dict=current.first->as<BoundaryValue::Dict>()){
   for(std::size_t i=0;i<dict->size();++i){const auto& row=(*dict)[i];
    for(std::size_t prior=0;prior<i;++prior)if((*dict)[prior].first==row.first){
     if(checkpoint_boundary_decode_active())checkpoint_decode_boundary_failure("DuplicateNavalJsonKey",{current.second,"/",row.first},ErrorKind::rule);
     return common_failure("DuplicateNavalJsonKey",checkpoint_decode_boundary_join({current.second,"/",row.first}));
    }
    pending.emplace_back(&row.second,checkpoint_decode_boundary_join({current.second,"/",row.first}));
   }
  }else if(const auto* array=current.first->as<BoundaryValue::Array>())for(std::size_t i=0;i<array->size();++i){
   char digits[3*sizeof(std::size_t)+1];const auto encoded=std::to_chars(digits,digits+sizeof(digits),i);
   pending.emplace_back(&(*array)[i],checkpoint_decode_boundary_join({current.second,"/",std::string_view(digits,encoded.ptr-digits)}));
  }
 }
 return common_success();
}
}
