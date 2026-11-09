#include "projection_composition_index.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 仅原组合错误码；资源失败沿allocator的拥有异常返回原job。
Status invalid(std::string_view code,std::string_view key={}){return {false,{},{{StableId(code),StableId(key)},ErrorKind::rule,{},{},{},{}}};}
}
ProjectionKeyIndex::ProjectionKeyIndex(std::shared_ptr<CheckpointMemoryResource> memory):memory_(std::move(memory)),keys_(CheckpointAllocator<Key>{memory_}){}
bool ProjectionKeyIndex::contains(std::string_view key)const noexcept{for(const auto& value:keys_)if(std::string_view(value.data(),value.size())==key)return true;return false;}
Status ProjectionKeyIndex::insert_unique(std::string_view key,std::string_view duplicate_code){if(contains(key))return invalid(duplicate_code,key);keys_.emplace_back(key.data(),key.size(),CheckpointAllocator<char>{memory_});return {true,{},{}};}
Status ProjectionKeyIndex::merge_unique(const ProjectionKeyIndex& source){for(const auto& key:source.rows()){auto status=insert_unique({key.data(),key.size()});if(!status.ok)return status;}return {true,{},{}};}
void ProjectionKeyIndex::set(std::string_view key){if(!contains(key))keys_.emplace_back(key.data(),key.size(),CheckpointAllocator<char>{memory_});}
Status ProjectionKeyIndex::validate_order(const FieldOrder& order)const{if(order.empty())return {true,{},{}};if(order.size()!=keys_.size())return invalid("BoundaryFieldOrderMismatch");for(std::size_t i=0;i<order.size();++i){if(!contains(order[i]))return invalid("BoundaryFieldOrderMismatch");for(std::size_t j=0;j<i;++j)if(order[j]==order[i])return invalid("BoundaryFieldOrderMismatch");}return {true,{},{}};}
ProjectionCompositionIndex::ProjectionCompositionIndex(std::shared_ptr<CheckpointMemoryResource> memory):roots(memory),action_params(memory),facts(memory),battles(std::move(memory)){}
Status ProjectionCompositionIndex::field_seen(std::size_t depth,std::string_view parent,std::string_view name){if(depth==1)return roots.insert_unique(name,"DuplicateBoundaryKey");if(depth==2){if(parent=="action_params")return action_params.insert_unique(name);if(parent=="facts")return facts.insert_unique(name);if(parent=="battles")return battles.insert_unique(name);}return {true,{},{}};}
}
