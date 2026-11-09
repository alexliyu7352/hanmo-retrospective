// HMCB2只作绑定；HMCN字段由原唯一typed codec处理，世界交叉核验仍在私有候选。
#include "continuation_binding.hpp"
#include <picosha2.h>
#include <algorithm>
#include <cstring>
namespace hanmo::resident {
namespace {
Status invalid(const char* code){return {false,{},{{code,"continuation"},ErrorKind::rule,{},{},{},{}}};}
class BindingRead final:public CheckpointReadSource {
public:
    BindingRead(CheckpointReadSource& source,std::uint64_t begin,std::uint64_t length,std::size_t chunk):source_(source),begin_(begin),length_(length),chunk_(chunk){}
    std::uint64_t size()const override{return length_;}
    Status read(std::uint64_t offset,std::uint8_t* data,std::size_t n)override{
        if(offset!=at_||offset>length_||n>length_-offset)return invalid("ContinuationReadOrderMismatch");
        for(std::size_t done=0;done<n;){auto take=std::min({n-done,chunk_,std::size_t(64)});auto s=source_.read(begin_+offset+done,data+done,take);if(!s.ok)return s;hash_.process(data+done,data+done+take);done+=take;}at_+=n;return {true,{},{}};
    }
    bool matches(const CheckpointDigest& digest){if(at_!=length_)return false;hash_.finish();CheckpointDigest actual{};hash_.get_hash_bytes(actual.begin(),actual.end());return actual==digest;}
private:CheckpointReadSource& source_;std::uint64_t begin_,length_,at_=0;std::size_t chunk_;picosha2::hash256_one_by_one hash_;
};
}
CheckpointJobResult<CheckpointJobBytes> bind_continuation_checkpoint_v2(const ContinuationSnapshot& value,const CheckpointDigest& digest,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    auto raw=encode_continuation_snapshot(value,capacity,slot,job);if(!raw.ok)return {false,{},std::move(raw.error)};return bind_encoded_continuation_checkpoint_v2(raw.value,digest,slot,job);
}
CheckpointJobStatus decode_bound_continuation_into_candidate(CheckpointReadSource& source,std::uint64_t begin,std::uint64_t length,const CheckpointDigest& checkpoint,const CheckpointDigest& descriptor,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CodecDecodeBudget& budget,CheckpointJobBudget& job,ContinuationSnapshot& output){
    try{auto profile=job.check_profile(capacity);if(!profile.ok)return profile;profile=job.check_profile(slot);if(!profile.ok)return profile;
        if(length<45||length>slot.max_continuation_bytes||begin>source.size()||length>source.size()-begin)return {false,{},job.capture_failure(invalid("InvalidContinuationLength").error)};
        auto scratch=job.reserve(CheckpointJobUse::scratch,1024);if(!scratch.ok)return {false,{},std::move(scratch.error)};BindingRead reader(source,begin,length,std::size_t(capacity.stream_chunk_bytes));std::uint8_t header[45]{};auto h=reader.read(0,header,45);if(!h.ok)return {false,{},job.capture_failure(std::move(h.error))};
        if(std::memcmp(header,"HMCB",4)||header[4]!=2)return {false,{},job.capture_failure(invalid("ContinuationVersionMismatch").error)};
        if(!std::equal(checkpoint.begin(),checkpoint.end(),header+5))return {false,{},job.capture_failure(invalid("ContinuationCheckpointMismatch").error)};
        std::uint64_t n=0;for(unsigned i=0;i<8;++i)n|=std::uint64_t(header[37+i])<<(8*i);if(n!=length-45)return {false,{},job.capture_failure(invalid("InvalidContinuationLength").error)};
        ContinuationSnapshot value;auto decoded=decode_continuation_snapshot_into_candidate(reader,45,n,capacity,slot,budget,job,value);if(!decoded.ok)return decoded;if(!reader.matches(descriptor))return {false,{},job.capture_failure(invalid("ContinuationSourceChanged").error)};output=std::move(value);return {true,{},{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
}
