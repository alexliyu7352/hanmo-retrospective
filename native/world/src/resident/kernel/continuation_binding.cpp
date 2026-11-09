// 双载荷先核摘要对应再解释有限许可；本层不安装世界或改变controller。
#include "continuation_binding.hpp"
#include "resident/model/common_model.hpp"
#include <picosha2.h>
namespace hanmo::resident {
namespace { CheckpointDigest checkpoint_raw_digest(const Bytes&); }
// 原typed许可作为内层，外层固定魔数与64位摘要只在冷保存边界生成。
Outcome<Bytes> bind_continuation_checkpoint(const ContinuationSnapshot& value,const Bytes& checkpoint){
    if(value.continuation_version!=1)return {false,{},common_failure("ContinuationVersionMismatch").error};
    auto encoded=encode_continuation_snapshot(value);if(!encoded.ok)return encoded;
    const auto digest=picosha2::bytes_to_hex_string(checkpoint_raw_digest(checkpoint));
    Bytes out{'H','M','C','B',1};out.insert(out.end(),digest.begin(),digest.end());out.insert(out.end(),encoded.value.begin(),encoded.value.end());
    if(out.size()>continuation_snapshot_byte_limit)return {false,{},common_failure("InvalidContinuationSnapshot").error};return {true,std::move(out),{}};
}
// 摘要绑定失败不尝试按另一世界重解释，内层仍严格拒绝超长或尾随字节。
Outcome<ContinuationSnapshot> unbind_continuation_checkpoint(const Bytes& bytes,const Bytes& checkpoint){
    const Bytes magic{'H','M','C','B',1};
    if(bytes.size()<=69||bytes.size()>continuation_snapshot_byte_limit||!std::equal(magic.begin(),magic.end(),bytes.begin()))return {false,{},common_failure("InvalidContinuationSnapshot").error};
    const auto digest=picosha2::bytes_to_hex_string(checkpoint_raw_digest(checkpoint));
    if(!std::equal(digest.begin(),digest.end(),bytes.begin()+5))return {false,{},common_failure("ContinuationCheckpointMismatch").error};
    // 旧外壳只接受旧整数HMCN1；升级内层分派不得让HMCB1偷渡精确续行。
    if(bytes.size()<74||bytes[69]!='H'||bytes[70]!='M'||bytes[71]!='C'||bytes[72]!='N'||bytes[73]!=1)return {false,{},common_failure("InvalidContinuationSnapshot").error};
    return decode_continuation_snapshot(Bytes(bytes.begin()+69,bytes.end()));
}
}

// 新绑定版本只接受原HMCN2 typed叶；raw摘要可由同一冻结checkpoint流的结果提供。
#include <limits>
namespace hanmo::resident {
namespace {
// 续行预算是部署技术限制，携带实际所需数/profile版本，不改写旧业务错误。
Status binding_capacity(const SlotCapacityProfile& profile,std::uint64_t needed){auto s=checkpoint_capacity_failure("continuation_bytes",needed,profile.profile_id,profile.revision);s.error.original.code="ContinuationCapacityExceeded";return s;}
Status binding_profile(const SlotCapacityProfile& p){auto s=validate_slot_capacity_profile(p);if(!s.ok)return s;if(p.max_continuation_bytes<=45||p.max_continuation_bytes>std::numeric_limits<std::size_t>::max())return binding_capacity(p,46);/* 全域关账：不再用估计的两份驻留内存约束真实续行；原 wire 长度门保留。 */return {true,{},{}};}
// PicoSHA2会copy一次process范围；小块喂入才能使整个checkpoint摘要无整档副本。
CheckpointDigest checkpoint_raw_digest(const Bytes& bytes){picosha2::hash256_one_by_one hash;for(std::size_t at=0;at<bytes.size();){const auto n=std::min<std::size_t>(64,bytes.size()-at);hash.process(bytes.data()+at,bytes.data()+at+n);at+=n;}hash.finish();CheckpointDigest digest{};hash.get_hash_bytes(digest.begin(),digest.end());return digest;}
}
Outcome<Bytes> bind_continuation_checkpoint_v2(const ContinuationSnapshot& value,const CheckpointDigest& digest,const SlotCapacityProfile& profile){
    try{auto checked=binding_profile(profile);if(!checked.ok)return {false,{},checked.error};if(value.continuation_version!=2)return {false,{},common_failure("ContinuationVersionMismatch").error};
        auto inner=encode_continuation_snapshot(value,static_cast<std::size_t>(profile.max_continuation_bytes-45));if(!inner.ok){if(inner.error.kind==ErrorKind::internal){inner.error.source_id=profile.profile_id;inner.error.limit=std::to_string(profile.revision);}return inner;}
        if(inner.value.size()>profile.max_continuation_bytes-45)return {false,{},binding_capacity(profile,inner.value.size()+45).error};
        Bytes result;result.reserve(inner.value.size()+45);result.insert(result.end(),{'H','M','C','B',2});result.insert(result.end(),digest.begin(),digest.end());
        const auto length=std::uint64_t(inner.value.size());for(unsigned i=0;i<8;++i)result.push_back(static_cast<std::uint8_t>(length>>(8*i)));result.insert(result.end(),inner.value.begin(),inner.value.end());return {true,std::move(result),{}};
    }catch(const std::bad_alloc&){return {false,{},common_failure("ContinuationResourceLimit",{},ErrorKind::internal).error};}
}
Outcome<ContinuationSnapshot> unbind_continuation_checkpoint_v2(const Bytes& bytes,const CheckpointDigest& digest,const SlotCapacityProfile& profile){
    try{auto checked=binding_profile(profile);if(!checked.ok)return {false,{},checked.error};if(bytes.size()>profile.max_continuation_bytes)return {false,{},binding_capacity(profile,bytes.size()).error};
        if(bytes.size()<=45||bytes[0]!='H'||bytes[1]!='M'||bytes[2]!='C'||bytes[3]!='B'||bytes[4]!=2)return {false,{},common_failure("InvalidContinuationSnapshot").error};
        if(!std::equal(digest.begin(),digest.end(),bytes.begin()+5))return {false,{},common_failure("ContinuationCheckpointMismatch").error};
        std::uint64_t length=0;for(unsigned i=0;i<8;++i)length|=std::uint64_t(bytes[37+i])<<(8*i);
        if(length!=bytes.size()-45||length<5||bytes[45]!='H'||bytes[46]!='M'||bytes[47]!='C'||bytes[48]!='N'||bytes[49]!=2)return {false,{},common_failure("InvalidContinuationSnapshot").error};
        return decode_continuation_snapshot(Bytes(bytes.begin()+45,bytes.end()),static_cast<std::size_t>(profile.max_continuation_bytes-45));
    }catch(const std::bad_alloc&){return {false,{},common_failure("ContinuationResourceLimit",{},ErrorKind::internal).error};}
}
Outcome<Bytes> bind_continuation_checkpoint(const ContinuationSnapshot& value,const Bytes& checkpoint,const SlotCapacityProfile& profile){
    auto checked=validate_slot_capacity_profile(profile);if(!checked.ok)return {false,{},checked.error};if(checkpoint.size()>profile.max_slot_bytes)return {false,{},binding_capacity(profile,checkpoint.size()).error};
    if(value.continuation_version==1)return bind_continuation_checkpoint(value,checkpoint);return bind_continuation_checkpoint_v2(value,checkpoint_raw_digest(checkpoint),profile);
}
Outcome<ContinuationSnapshot> unbind_continuation_checkpoint(const Bytes& bytes,const Bytes& checkpoint,const SlotCapacityProfile& profile){
    auto checked=validate_slot_capacity_profile(profile);if(!checked.ok)return {false,{},checked.error};if(checkpoint.size()>profile.max_slot_bytes)return {false,{},binding_capacity(profile,checkpoint.size()).error};
    if(bytes.size()>4&&bytes[4]==1)return unbind_continuation_checkpoint(bytes,checkpoint);return unbind_continuation_checkpoint_v2(bytes,checkpoint_raw_digest(checkpoint),profile);
}
}

namespace hanmo::resident {
// 只核wire外壳；typed HMCN语义须由原core核验，不能据此铸造准入/保存成功。
CheckpointJobResult<CheckpointJobBytes> bind_encoded_continuation_checkpoint_v2(const CheckpointJobBytes& inner,const CheckpointDigest& digest,const SlotCapacityProfile& profile,CheckpointJobBudget& job){try{
    auto checked=job.check_profile(profile);if(!checked.ok)return {false,{},std::move(checked.error)};
    if(!inner.belongs_to(job))return {false,{},job.capture_boundary_failure("CheckpointJobReservationMismatch",{},ErrorKind::internal)};
    const auto& bytes=inner.bytes();if(bytes.size()<5||bytes[0]!='H'||bytes[1]!='M'||bytes[2]!='C'||bytes[3]!='N'||bytes[4]!=2)return {false,{},job.capture_boundary_failure("InvalidContinuationSnapshot",{},ErrorKind::rule)};
    if(profile.max_continuation_bytes<45||bytes.size()>profile.max_continuation_bytes-45)return {false,{},job.capture_failure(binding_capacity(profile,bytes.size()>UINT64_MAX-45?UINT64_MAX:bytes.size()+45).error)};
    auto output=CheckpointJobBytes::allocate(job,CheckpointJobUse::slot,bytes.size()+45);if(!output.ok)return output;
    const std::uint8_t magic[]{'H','M','C','B',2};std::uint8_t length[8]{};for(unsigned i=0;i<8;++i)length[i]=std::uint8_t(std::uint64_t(bytes.size())>>(8*i));
    auto status=output.value.write(0,magic,5);if(status.ok)status=output.value.write(5,digest.data(),32);if(status.ok)status=output.value.write(37,length,8);if(status.ok)status=output.value.write(45,bytes.data(),bytes.size());if(!status.ok)return {false,{},std::move(status.error)};return output;
}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
CheckpointJobResult<CheckpointJobBytes> unbind_encoded_continuation_checkpoint_v2(const CheckpointJobBytes& input,const CheckpointDigest& digest,const SlotCapacityProfile& profile,CheckpointJobBudget& job){try{
    auto checked=job.check_profile(profile);if(!checked.ok)return {false,{},std::move(checked.error)};
    if(!input.belongs_to(job))return {false,{},job.capture_boundary_failure("CheckpointJobReservationMismatch",{},ErrorKind::internal)};
    const auto& bytes=input.bytes();if(bytes.size()>profile.max_continuation_bytes)return {false,{},job.capture_failure(binding_capacity(profile,bytes.size()).error)};
    if(bytes.size()<50||bytes[0]!='H'||bytes[1]!='M'||bytes[2]!='C'||bytes[3]!='B'||bytes[4]!=2)return {false,{},job.capture_boundary_failure("InvalidContinuationSnapshot",{},ErrorKind::rule)};
    if(!std::equal(digest.begin(),digest.end(),bytes.begin()+5))return {false,{},job.capture_boundary_failure("ContinuationCheckpointMismatch",{},ErrorKind::rule)};
    std::uint64_t length=0;for(unsigned i=0;i<8;++i)length|=std::uint64_t(bytes[37+i])<<(8*i);
    if(length!=bytes.size()-45||bytes[45]!='H'||bytes[46]!='M'||bytes[47]!='C'||bytes[48]!='N'||bytes[49]!=2)return {false,{},job.capture_boundary_failure("InvalidContinuationSnapshot",{},ErrorKind::rule)};
    auto output=CheckpointJobBytes::allocate(job,CheckpointJobUse::slot,length);if(!output.ok)return output;auto status=output.value.write(0,bytes.data()+45,static_cast<std::size_t>(length));if(!status.ok)return {false,{},std::move(status.error)};return output;
}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
}
