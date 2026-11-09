#include "hanmo/resident/checkpoint_memory.hpp"
// P1 CLI/薄Bridge共用唯一常驻core；本头不要求Godot、Flecs或物理存储服务。
#pragma once
#include "hanmo/resident/phase_contract.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
// 各域typed命令union与完整回执在对应固定合同头定义，不能用整世界Value::Dict替代。
struct CommandPayload; struct JourneyViewPage;
struct CommandEnvelope {
    WorldId world_id{}; Integer execution_epoch=0,request_seq=0,expected_revision=0;
    StableId actor_id; std::shared_ptr<const CommandPayload> payload;
};
// begin只建立停驻许可；Main每次只授予1半日，CLI才可明确授予多步压力预算。
// poll/cancel也核世界与epoch；恢复前旧回调不能碰重用的run_id。
struct RunRef { WorldId world_id{}; Integer execution_epoch=0; StableId run_id; };
// /2真实目标联合；Plan范围版本由原owner递增，不能以world revision代替。
struct ActionRunDriveTarget { StableId action_id,action_kind; };
struct PlanRunDriveTarget { StableId plan_id;Integer approved_scope_revision=0; };
using RunDriveTarget=std::variant<ActionRunDriveTarget,PlanRunDriveTarget>;
struct BeginRun { WorldIdentity expected; StableId run_id,actor_id,action_id; Integer max_half_days=0,max_gross_outflow=0; bool resume_saved_scope=false; StableId plan_id;std::optional<RunDriveTarget> target; };
struct ResumeRun { WorldIdentity expected; StableId run_id; Integer permit_seq=0,grant_half_days=0; };
enum class RunStop { awaiting_permit,awaiting_input,arrived,paused,stranded,blocked,cancelled,budget_exhausted,failed,money_budget_exhausted,action_replaced,restored,completed };
// 新profile下exact_spent/remaining为真实预算，旧半日字段只保兼容整数展示。
struct RunProgress { WorldIdentity identity; StableId run_id; Integer committed_half_days=0,permit_seq=0; RunStop stop=RunStop::awaiting_permit; std::optional<RuleError> error; Integer gross_outflow=0,remaining_half_days=0,remaining_gross_outflow=0; StableId actor_id,action_id,action_kind,wallet_id; Integer max_half_days=0,max_gross_outflow=0; StableId plan_id; std::optional<WorkUnits> exact_spent_units,exact_remaining_units; std::optional<WorldMoment> exact_source_moment; };
// gate只读真实原目标及预算，不携带可执行PreparedDomainOp或新增许可。
enum class RunDriveNext { none,original_command,world_half_day };
struct RunDriveGate { RunDriveTarget target;WorldIdentity source_identity;std::optional<WorldMoment> exact_source_moment;RunDriveNext next=RunDriveNext::none;RunStop stop=RunStop::blocked;StableId reason;std::optional<StableId> step_ref,bound_action_ref;RunProgress budget_projection; };
// 保存的停驻许可只绑定原行动与已用预算；不是世界、钱包或可重用的执行epoch。
// HMCN2使用原实际Started来源；schema外置，不能将临时run ID当业务来源。
struct ContinuationStartedSource { StableId domain,kind,id; std::uint32_t schema=0; };
struct ContinuationSnapshot { WorldIdentity source_identity; StableId actor_id,action_id,action_kind,wallet_id; Integer action_start_tick=0,started_tick=0,max_half_days=0,max_gross_outflow=0,committed_half_days=0,gross_outflow=0; RunStop stop=RunStop::cancelled; Integer continuation_version=1; std::optional<WorldMoment> exact_source_moment,exact_started_at; std::optional<HalfDayDuration> exact_committed_duration; std::optional<ContinuationStartedSource> action_started_source; };
// 只有初始化/checkpoint/debug完整导出使用bytes；热命令/phase/view不解析全世界JSON。
using Bytes=std::vector<std::uint8_t>;
class JsonDecodedWorldSeedOwner;class WorldRestoreRetention;class CommittedProfileDeliveries;class WorldCheckpointCapture;struct CapacityProfile;struct SlotCapacityProfile;class CapturedContinuation;class WorldRestoreCandidate;class WorldInstallCancellation;class CheckpointJobBudget;
template<class T>struct CheckpointJobResult;
class ResidentWorld : public CheckpointAllocatedObject {
public:
    ResidentWorld(); ~ResidentWorld();
    ResidentWorld(const ResidentWorld&)=delete;
    ResidentWorld& operator=(const ResidentWorld&)=delete;
    Status install(std::shared_ptr<const PreparedCatalog>,InitialWorldSeed&&,bool fresh_native=false);
    Outcome<WorldIdentity> identity() const;
    Outcome<std::shared_ptr<const PreviewResult>> preview(const CommandEnvelope&) const;
    Outcome<std::shared_ptr<const CommitResult>> submit(const CommandEnvelope&);
    Outcome<RunProgress> begin_run(const BeginRun&);
    Outcome<RunProgress> resume_run(const ResumeRun&);
    Outcome<RunProgress> poll_run(const RunRef&) const;
    Outcome<RunProgress> cancel_run(const RunRef&);
    Outcome<RunDriveGate> read_run_drive_gate(const RunRef&) const;
    Outcome<std::optional<ContinuationSnapshot>> continuation_snapshot() const;
    Status restore_continuation(const ContinuationSnapshot&);
    Outcome<std::shared_ptr<const JourneyViewPage>> journey_view(const StableId& actor_id,Integer cursor=0) const;
    Outcome<Bytes> checkpoint();
    // 捕获仍由原run投影产生；拥有结果携同job字符串和BigNat租约。
    CheckpointJobResult<CapturedContinuation> capture_continuation_for_checkpoint(CheckpointJobBudget&)const;
    CheckpointJobResult<CommittedProfileDeliveries> capture_profile_deliveries(const CapacityProfile&,CheckpointJobBudget&)const;
    // 封闭旧world保留租约；实际源与同job账由原世界取得。
    CheckpointJobResult<std::shared_ptr<const WorldRestoreRetention>> retain_storage_world(const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
    bool storage_restore_pending()const noexcept;
    std::shared_ptr<const WorldCheckpointCapture> active_storage_capture()const noexcept;
    CheckpointJobResult<std::shared_ptr<const WorldCheckpointCapture>> capture_storage_checkpoint(const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
    CheckpointJobResult<WorldIdentity> install_checkpoint_candidate(WorldRestoreCandidate&&,const std::optional<WorldIdentity>& expected_active,WorldInstallCancellation&,CheckpointJobBudget&);
    Status restore(const Bytes&,std::shared_ptr<const PreparedCatalog>,const std::optional<ContinuationSnapshot>& continuation={});
private:
    friend class NativeBoundarySession;
    Status install_json_seed(std::shared_ptr<const PreparedCatalog>,JsonDecodedWorldSeedOwner&&);
    Status install_seed(std::shared_ptr<const PreparedCatalog>,InitialWorldSeed&,bool,JsonDecodedWorldSeedOwner*);
    struct Impl; std::unique_ptr<Impl> impl_;
};
inline constexpr std::size_t journey_page_limit=64;
} // 失败恢复不替换现world；恢复成功换epoch；run授权不进checkpoint。
