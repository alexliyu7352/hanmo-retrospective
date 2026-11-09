// 本机准入与槽胶囊的唯一typed值；胶囊只绑定身份和基线，绝不授予本机历史权限。
#pragma once
#include "hanmo/resident_world_api.hpp"
#include "resident/checkpoint/checkpoint_stream.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/kernel/profile_delivery.hpp"
#include <optional>

namespace hanmo::resident::storage {
enum class ProfilePurpose : std::uint8_t { formal=0,author_preview=1,development=2 };
enum class AdmissionOrigin : std::uint8_t { local_created=0,imported=1 };
// namespace由真实宿主入口选择；不能从外来胶囊继承为当前权限。
struct ProfileNamespace {
    StableId profile_uid;
    ProfilePurpose purpose=ProfilePurpose::formal;
    std::optional<StableId> preview_session_id;
};
using ::hanmo::resident::DeliveryKey;
using ::hanmo::resident::EntryFingerprint;
// 按S§3原字段顺序；HMAC只为格式magic，外来摘要不证明同意。
struct SlotAdmissionCapsule {
    StableId admission_id;
    ProfileNamespace name_space;
    WorldId source_world_id{};
    CheckpointDigest origin_snapshot_sha256{};
    std::vector<EntryFingerprint> baseline;
    CheckpointDigest baseline_sha256{};
};
// allowed_history与origin只存在受信本机协调记录，不进入槽或外来导入文件。
struct AdmissionRecord {
    SlotAdmissionCapsule capsule;
    AdmissionOrigin origin=AdmissionOrigin::imported;
    std::vector<EntryFingerprint> allowed_history;
    Integer record_generation=0;
};
struct SlotWriteRequest;
class ResidentProfileCoordinator;
class ResidentStorageService;
// 只有真实coordinator核完原创建回执与冻结代后可铸造；普通调用者不能传shape/hash自行发布。
class ProfileValidatedStage {
public:
    ProfileValidatedStage(const ProfileValidatedStage&)=delete;
    ProfileValidatedStage& operator=(const ProfileValidatedStage&)=delete;
    ProfileValidatedStage(ProfileValidatedStage&& other)noexcept
        :frozen_(std::move(other.frozen_)),instance_(other.instance_),operation_(other.operation_),stage_(other.stage_),binding_(std::move(other.binding_)),expected_generation_(other.expected_generation_),new_generation_(other.new_generation_),admission_id_(std::move(other.admission_id_)),consumed_(other.consumed_){other.consumed_=true;}
private:
    friend class ResidentProfileCoordinator;
    friend class ResidentStorageService;
    ProfileValidatedStage(std::shared_ptr<const SlotWriteRequest> frozen,std::uint64_t instance,std::uint64_t operation,std::uint64_t stage,StableId binding,Integer expected,Integer next,StableId admission)
        :frozen_(std::move(frozen)),instance_(instance),operation_(operation),stage_(stage),binding_(std::move(binding)),expected_generation_(expected),new_generation_(next),admission_id_(std::move(admission)){}
    std::shared_ptr<const SlotWriteRequest> frozen_;
    std::uint64_t instance_=0,operation_=0,stage_=0;
    StableId binding_;
    Integer expected_generation_=0,new_generation_=0;
    StableId admission_id_;
    bool consumed_=false;
};
inline constexpr std::size_t admission_fingerprint_limit=16384;
// 固定PicoSHA2小块喂入的保守scratch预算（含vector短暂扩容），不随输入文件增长。
// 预算由宿主配置给出；约5MiB完整合法胶囊不能被固定小字符串限制截断。
struct AdmissionCodecLimits { std::uint64_t max_capsule_bytes=0; };
bool same_profile_namespace(const ProfileNamespace&,const ProfileNamespace&) noexcept;
bool same_delivery_key(const DeliveryKey&,const DeliveryKey&) noexcept;
bool same_entry_fingerprint(const EntryFingerprint&,const EntryFingerprint&) noexcept;
bool same_admission_capsule(const SlotAdmissionCapsule&,const SlotAdmissionCapsule&) noexcept;
Status validate_profile_namespace(const ProfileNamespace&);
Status validate_admission_record(const AdmissionRecord&,const AdmissionCodecLimits&);
// baseline摘要仅覆盖count和每行完整规范字节；不排序、不修复输入。
Outcome<CheckpointDigest> admission_baseline_digest(const WorldId&,const std::vector<EntryFingerprint>&,const AdmissionCodecLimits&);
Outcome<Bytes> encode_slot_admission_capsule(const SlotAdmissionCapsule&,const AdmissionCodecLimits&);
Outcome<SlotAdmissionCapsule> decode_slot_admission_capsule(const Bytes&,const AdmissionCodecLimits&);
// 胶囊深值和同job分配预留共同移动；外部只读借用，不能把未核字段当准入权。
class JobAdmissionCapsule {
public:
    JobAdmissionCapsule()=default;
    JobAdmissionCapsule(const JobAdmissionCapsule&)=delete;
    JobAdmissionCapsule& operator=(const JobAdmissionCapsule&)=delete;
    JobAdmissionCapsule(JobAdmissionCapsule&&) noexcept=default;
    JobAdmissionCapsule& operator=(JobAdmissionCapsule&&) noexcept;
    const SlotAdmissionCapsule& value()const noexcept{return value_;}
    bool belongs_to(const CheckpointJobBudget& job)const noexcept{return reservation_.belongs_to(job);}
private:
    friend struct JobAdmissionAccess;
    CheckpointJobReservation reservation_;
    SlotAdmissionCapsule value_;
};
// 旧格式同一writer；显式入口先核profile与源保留量，再直接写入唯一拥有输出。
CheckpointJobResult<CheckpointJobBytes> encode_slot_admission_capsule(
    const SlotAdmissionCapsule&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// 封闭解码拥有者：同 job 不重复收费；跨 job 保原诊断控制与真实字段直到同步编码结束。
CheckpointJobResult<CheckpointJobBytes> encode_slot_admission_capsule(
    const JobAdmissionCapsule&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
CheckpointJobResult<JobAdmissionCapsule> decode_slot_admission_capsule(
    const CheckpointJobBytes&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// worker在原已验段范围内读取；两遍各对实际解释字节核descriptor，不复制整胶囊输入。
CheckpointJobResult<JobAdmissionCapsule> decode_slot_admission_capsule(
    CheckpointReadSource&,std::uint64_t begin,std::uint64_t length,const CheckpointDigest&,
    const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// META原字段共用同一个writer/reader；sink为空只测量，不建临时胶囊Bytes。
CheckpointJobResult<std::uint64_t> write_slot_admission_capsule_to(CheckpointWriteSink*,const SlotAdmissionCapsule&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
CheckpointJobResult<SlotAdmissionCapsule> decode_slot_admission_capsule_into_job(CheckpointReadSource&,std::uint64_t,std::uint64_t,const CheckpointDigest&,const CapacityProfile&,const SlotCapacityProfile&,CodecDecodeBudget&,CheckpointJobBudget&);
// 冷核同时比较实际checkpoint的world、宿主选择及本机记录；缺record不得凭胶囊自恢复权限。
Status verify_local_admission(const SlotAdmissionCapsule&,const WorldId& checkpoint_world,const ProfileNamespace& host_namespace,const AdmissionRecord*,const AdmissionCodecLimits&);
CheckpointDigest profile_sha256(const std::uint8_t*,std::size_t);
StableId profile_digest_hex(const CheckpointDigest&);
Outcome<StableId> profile_namespace_binding(const ProfileNamespace&);
Status profile_admission_failure(const char*,const StableId& path={},ErrorKind=ErrorKind::rule);
}
