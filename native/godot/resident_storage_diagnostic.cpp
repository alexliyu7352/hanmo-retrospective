#include "resident_storage_diagnostic.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace hanmo::resident::storage {
struct StorageDiagnostic::LongText {
    CheckpointVector<char> bytes;
    explicit LongText(std::string_view value){
        if(value.size()==std::numeric_limits<std::size_t>::max())throw std::bad_alloc();
        bytes.resize(value.size()+1); // actual allocator charge precedes every allocation
        std::memcpy(bytes.data(),value.data(),value.size());bytes.back()='\0';
    }
};
StorageDiagnostic::StorageDiagnostic(std::string_view value){
    if(value.size()<inline_.size()){
        if(!value.empty())std::memcpy(inline_.data(),value.data(),value.size());
        inline_size_=value.size();inline_[inline_size_]='\0';
    }else long_=checkpoint_make_shared<LongText>(value);
}
StorageDiagnostic StorageDiagnostic::from_media(const SlotMediaError& error)noexcept{
    StorageDiagnostic result;result.media_.emplace(error);return result;
}
const char* StorageDiagnostic::c_str()const noexcept{
    if(media_)return media_->what();if(long_)return long_->bytes.data();return inline_.data();
}
std::string_view StorageDiagnostic::view()const noexcept{
    if(media_)return std::string_view(media_->what());
    if(long_)return {long_->bytes.data(),long_->bytes.size()-1};return {inline_.data(),inline_size_};
}
void StorageDiagnostic::retain(CheckpointRetentionCensus& census)const{
    // media_ 是原 SlotMediaError 的 owning 副本，不再因无内存计量凭据拒绝。
    if(media_)return;
    if(!long_)return;census.owner(checkpoint_shared_allocation_owner(long_));
    if(census.visit_shared_object(long_.get()))census.vector(long_->bytes);
}
struct StorageCoreError::State {
    CheckpointMemoryCharge memory;RuleError value;
    State(CheckpointMemoryCharge reserved,const RuleError& source):memory(std::move(reserved)),value(source){}
};
StorageCoreError::StorageCoreError(const RuleError& value){
    auto memory=CheckpointMemoryCharge(0);state_=checkpoint_make_shared<State>(std::move(memory),value);
}
const RuleError* StorageCoreError::get()const noexcept{return state_?&state_->value:nullptr;}
void StorageCoreError::retain(CheckpointRetentionCensus& census)const{
    if(!state_)return;census.owner(checkpoint_shared_allocation_owner(state_));
    if(!census.visit_shared_object(state_.get()))return;census.resource(state_->memory.resource());
}
}
