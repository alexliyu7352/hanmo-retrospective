// CLI与薄Bridge的共同冷边界；不定义第二份领域规则或独立世界，真实实现由中央组合唯一ResidentWorld。
#pragma once
#include "hanmo/resident_world_api.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "profile_delivery_capture.hpp"
#include "world_checkpoint_capture.hpp"
namespace hanmo::resident {
class FieldBattleResourceRead;
// 整条命令处理包含解码/预览/提交/回执DTO；完整状态export只在计时外oracle/checkpoint发生。
struct ReplayCommandTiming { std::uint64_t decode_usec=0,preview_usec=0,submit_usec=0,dto_usec=0; std::vector<std::pair<StableId,Integer>> stages; };
struct ReplayCommandResult { BoundaryValue preview,receipt; ReplayCommandTiming timing;
    // A capacity failure is borrowed only while this original lease remains alive.
    CheckpointJobFailureLease checkpoint_failure;
    std::shared_ptr<const WorldCheckpointCapture> checkpoint_capture;
};
// P2的闭合小操作沿用同一session；payload只含当前命令/查询/许可字段，绝不是世界快照。
enum class LiveOperationKind { preview,command,query,begin_run,grant,cancel_run,checkpoint,restore };
struct LiveOperation {
    LiveOperationKind kind=LiveOperationKind::query; WorldIdentity source; BoundaryValue payload;
    // 仅冷restore/checkpoint携带；世界blob上限64MiB、许可2KiB，不能混进文本或逐字节整数数组。
    std::optional<Bytes> checkpoint_bytes{},previous_checkpoint_bytes{},continuation_bytes{},previous_continuation_bytes{};
};
struct LiveOperationResult {
    WorldIdentity source_identity,current_identity; BoundaryValue result;
    std::optional<Bytes> checkpoint_bytes{},continuation_bytes{};
    CheckpointJobFailureLease checkpoint_failure;
    std::shared_ptr<const WorldCheckpointCapture> checkpoint_capture;
};
inline constexpr std::size_t live_checkpoint_byte_limit=64u*1024u*1024u;
// 槽核验只回传实际输入摘要与来源/当前身份；不是世界安装许可或磁盘耐久回执。
struct StorageCheckpointValidation {
    WorldIdentity source_identity,current_identity;
    StableId candidate_sha256,candidate_continuation_sha256,previous_sha256,previous_continuation_sha256;
};
// 仅已选无取得producer的完整工程profile可给此只读证明；不是通用空outbox或创建许可。
struct ProfileDeliveryAbsenceProof {
    WorldIdentity source_identity;
    CatalogIdentity catalog;
    StableId profile_id;
};
class WorldRestoreRetention;class WorldRestoreCandidate;class WorldInstallCancellation;
class ResidentReplaySession {
public:
    virtual ~ResidentReplaySession()=default;
    // 一次目录/初态独立校验后创建常驻core；失败不得改变已安装世界。
    virtual Status initialize(const BoundaryValue& catalog,const BoundaryValue& state)=0;
    // 新profile冷启动/冷恢复均先完整验证；协议fixture未实现时明确拒绝。
    virtual Status initialize_native(const BoundaryValue&,const BoundaryValue&){return {false,{},{{"UnsupportedNativeInitialization",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
    virtual Status initialize_checkpoint(const BoundaryValue&,const Bytes&){return {false,{},{{"UnsupportedCheckpointInstallation",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
    virtual Status initialize_checkpoint_with_continuation(const BoundaryValue& catalog,const Bytes& bytes,const Bytes& continuation){if(continuation.empty())return initialize_checkpoint(catalog,bytes);return {false,{},{{"UnsupportedContinuationInstallation",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
    virtual Outcome<BoundaryValue> installed_selection()const{return {false,{},{{"UnsupportedInstalledSelection",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}

    // 目录沿原native内容锁/情景工厂生成；槽文件不能授予缺失module/provider。
    // 当前宿主唯一载入期租约；空宿主同样阻止中途创建第二个活动世界。
    virtual CheckpointJobResult<std::shared_ptr<const WorldRestoreRetention>> retain_storage_world(const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
    virtual CheckpointJobResult<std::shared_ptr<const PreparedCatalog>> prepare_storage_catalog(const BoundaryValue&,const StableId& scenario,const CatalogIdentity& expected,CheckpointJobBudget&)const;
    // 完整槽入口只接原decoder封闭候选；预期当前身份与最后取消仍在原ResidentWorld发布点核。
    virtual CheckpointJobResult<WorldIdentity> install_storage_candidate(WorldRestoreCandidate&&,const std::optional<WorldIdentity>& expected_active,WorldInstallCancellation&,CheckpointJobBudget&);
    // 唯一已提交世界生成原取得投影，不收宿主自报来源或显示卡列表。
    virtual CheckpointJobResult<CommittedProfileDeliveries> capture_profile_deliveries(const CapacityProfile&,CheckpointJobBudget&)const;
    virtual CheckpointJobResult<std::shared_ptr<const WorldCheckpointCapture>> capture_storage_checkpoint(const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);

    // 仅未安装冷选择；返回有限目录/情景身份，完整restore仍必须验证所有域。
    virtual Outcome<BoundaryValue> inspect_checkpoint_selection(const Bytes&){return {false,{},{{"UnsupportedCheckpointSelection",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
    // Envelope形状先核；业务params错误必须在原actor/重放/revision/领域guard位置消费。
    // 预览失败不取消submit的原重放行为；普通命令拒绝仍输出完整receipt，外层Outcome只报调用失败。
    virtual Outcome<ReplayCommandResult> execute(const BoundaryValue& command)=0;
    // 只读完整原语义状态用于oracle；不在poll、普通DTO或每个热查询中自动调用。
    virtual Outcome<BoundaryValue> export_state() const=0;
    virtual Outcome<WorldIdentity> identity() const=0;
    virtual Outcome<Bytes> checkpoint()=0;
    virtual Status restore(const Bytes&)=0;
    // 实际实现保持caller身份、pure preview及唯一core许可；旧P1协议fixture明确不具P2能力。
    virtual Outcome<LiveOperationResult> execute_live(const LiveOperation&){return {false,{},{{"UnsupportedLiveOperation",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
    // 仅所属world线程只读复核原冻结候选/旧代；当前同epoch可已前进，候选持久身份仍须精确匹配source。
    virtual Outcome<StorageCheckpointValidation> validate_storage_checkpoint(const WorldIdentity&,const Bytes&,const std::optional<Bytes>&,const std::optional<Bytes>&,const std::optional<Bytes>&)const{return {false,{},{{"UnsupportedStorageCheckpointValidation",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
    // 仅复现原oracle明确的一次晚失败；正常构建永不授予该测试能力，Godot不注册此方法。
    virtual Status arm_late_failure_probe(){return {false,{},{{"TestFaultInjectionDisabled",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
    // 证明与调用时同一已提交world/revision/epoch/catalog绑定；未实现或有来源时明确拒绝。
    virtual Outcome<ProfileDeliveryAbsenceProof> inspect_profile_delivery_absence()const{return {false,{},{{"UnsupportedProfileDeliveryProjection",{}},ErrorKind::unsupported_capability,{},{},{},{}}};}
};
// 中央factory只构造一个真实core适配器；尚未链接实现时由显式构建入口报告未支持。
std::unique_ptr<ResidentReplaySession> create_resident_replay_session();
// 显式资源宿主入口；旧零参只能用于未选择 field_battle 的 profile。
std::unique_ptr<ResidentReplaySession> create_resident_replay_session(std::shared_ptr<const FieldBattleResourceRead>);
} // 不提供提前返回领域参数错误的第二decode_command入口。
