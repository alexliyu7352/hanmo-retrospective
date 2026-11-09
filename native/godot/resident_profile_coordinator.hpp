// 宿主唯一active session的本机准入协调者；世界提交与元档案持久分别回执，不创建镜像世界。
#pragma once
#include "resident_profile_store.hpp"
#include "resident_profile_merge.hpp"
#include "resident_world_import.hpp"
#include "resident_storage_service.hpp"
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/world_restore_candidate.hpp"
#include <thread>

namespace hanmo::resident::storage {
enum class ProfileAdmissionPhase { idle,proving_creation,reading,staging,validating,publishing,awaiting_recovery,stored,blocked,closed };
// 此状态不声称META奖励投递已经实现；blocked世界仍可真实存在。
struct ProfileAdmissionStatus {
    bool world_committed=false,admission_stored=false,profile_sync_blocked=true;
    ProfileAdmissionPhase phase=ProfileAdmissionPhase::idle;
    SlotDurability durability=SlotDurability::not_published;
    StorageDiagnostic code,admission_id;
    WorldIdentity source;
    Integer profile_generation=0;int system_error=0;
    CheckpointJobFailureLease job_failure;
    CheckpointJobFailureCode fixed_failure=CheckpointJobFailureCode::none;
};
// 只在原META事务读回且耐久后公开；三类逐entry结果属于同一代，不写回世界。
struct ProfileDeliveryReceipt {
    CheckpointMemoryCharge owned_memory;
    WorldIdentity committed_projection_identity;
    Integer profile_generation=0,record_generation=0;
    CheckpointDigest verified_digest{};
    std::vector<StableId> accepted_delivery_ids,already_present_ids,historical_quarantined_ids;
};
struct CapturedProfileAdmission {
    WorldIdentity source_identity;
    SlotAdmissionCapsule capsule;
    bool profile_sync_blocked=true;
};
// 异步保存只交唯一拥有字节，源世界身份与该次原胶囊不可另行替换。
struct CapturedProfileAdmissionBytes {
    WorldIdentity source_identity;
    CheckpointJobBytes capsule;
    bool profile_sync_blocked=true;
};
struct ProfileWorldInstallResult {WorldIdentity source;Integer profile_generation=0;bool admission_stored=false,profile_sync_blocked=true;};
// 所有方法由宿主world线程调用；实际I/O仍由原ResidentStorageService worker执行。
class ResidentProfileCoordinator {
public:
    ResidentProfileCoordinator(ResidentReplaySession& active_session,ProfileNamespace host_namespace,std::string profile_directory,ProfileStoreLimits,StorageServiceLimits
#ifdef HANMO_STORAGE_TEST_FAULTS
        ,SlotIoTestHooks hooks={}
#endif
    );
    ~ResidentProfileCoordinator();
    ResidentProfileCoordinator(const ResidentProfileCoordinator&)=delete;
    bool owns_session(const ResidentReplaySession& value)const noexcept{return &session_==&value;}
    // 首次只读本机元档案；原创建retry只继续原记录；冷恢复已关联后明确拒普通重读。
    Status begin_read();
    // 只调用注入的同一session initialize_native；只有该调用真实成功才形成内部创建回执。
    ProfileAdmissionStatus create_local_world(const BoundaryValue& catalog,const BoundaryValue& native_request);
    // 显式外来导入；Prepare不写世界/profile，Accept重新读取同路径/全档摘要并完整冷核。
    StorageAccepted prepare_world_import(const BoundaryValue& catalog,const StableId& scenario,const CatalogIdentity& expected_catalog,const std::string& source_path,const CheckpointDigest& source_bytes_sha256);
    WorldImportStatus import_status()const;
    Status accept_world_import(const WorldImportPreviewIdentity&,const ImportHistoryChoice&);
    Status advance_world_import();
    // 只在原准入介质真实失败后接受此明确确认；unknown仍先恢复同字节屏障。
    Status confirm_import_without_profile(const WorldImportPreviewIdentity&);
    Status retry_world_import_admission(const WorldImportPreviewIdentity&);
    Status cancel_world_import(const WorldImportPreviewIdentity&);
    Status release_world_import(const WorldImportPreviewIdentity&);
    // 重试仅补当前原创建回执；未知发布结果先补同字节屏障，不生成下一代洗掉未知结果。
    Status retry_admission();
    // 仅真正session封闭capture的已提交来源可触发META独立事务；等待I/O不改世界。
    Status begin_merge(const CommittedProfileDeliveries&,Integer expected_profile_generation);
    // /2原调用自行重取同session已提交投影，显式核record与profile两代。
    Status begin_merge(Integer expected_record_generation,Integer expected_profile_generation);
    Status begin_grant_imported_history(const StableId& admission_id,Integer expected_record_generation,Integer expected_profile_generation,const ImportHistoryChoice&);
    std::shared_ptr<const ProfileDeliveryReceipt> delivery_receipt()const noexcept;
    // 本机显式历史选择只授原baseline内完整fingerprint，绝不改外来胶囊。
    Status begin_authorize_history(const StableId& admission_id,const std::vector<EntryFingerprint>&,Integer expected_profile_generation);
    Status advance();
    ProfileAdmissionStatus status()const;
    Status cancel();
    void request_close();
    bool close_complete()const;
    // 已有本机record的冷载入沿原核心完整校验/一次publish；缺record可载真实世界但明确blocked。
    ProfileAdmissionStatus restore_local_world(const BoundaryValue& catalog,const Bytes& checkpoint,const Bytes& continuation,const Bytes& capsule);
    // S§3缺本机record保真实世界/完整胶囊但阻profile同步；介质失败导入的确认分支不走此口。
    CheckpointJobResult<ProfileWorldInstallResult> install_local_candidate(WorldRestoreCandidate&&,JobAdmissionCapsule&&,const std::optional<WorldIdentity>& expected_active,WorldInstallCancellation&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
    // 纯捕获同一活动世界关联；更换epoch后必须显式restore关联，不自动清空baseline。
    Outcome<CapturedProfileAdmission> capture(const WorldIdentity& same_snapshot)const;
    CheckpointJobResult<CapturedProfileAdmissionBytes> capture_bytes(const WorldIdentity& same_snapshot,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&)const;
    // 有限查询只回同一host namespace的本机记录副本，未loaded/不存在不会返回假record。
    Outcome<AdmissionRecord> read_admission(const StableId&)const;
private:
    ResidentReplaySession& session_;
    ProfileNamespace host_namespace_;
    StableId namespace_binding_;
    ProfileStoreLimits limits_;
    ResidentStorageService storage_;
    std::thread::id owner_;
    ProfileAdmissionStatus status_;
    std::optional<StorageOperationRef> operation_;
    bool reading_=false,loaded_=false,closing_=false;
    std::shared_ptr<const SlotBankPair> banks_;
    std::shared_ptr<const JobSlotBankPair> banks_ownership_;
    ProfileMetadataSelected selected_;
    CheckpointMemoryCharge creation_memory_;
    std::optional<AdmissionRecord> creation_;
    std::optional<WorldIdentity> creation_identity_;
    bool creation_proven_=false;
    std::optional<ProfileMetadataSnapshot> next_;
    std::shared_ptr<const SlotBankPair> next_banks_;
    std::shared_ptr<const JobSlotBankPair> next_banks_ownership_;
    std::shared_ptr<ProfileDeliveryReceipt> pending_delivery_receipt_;
    std::shared_ptr<const ProfileDeliveryReceipt> delivered_receipt_;
    struct ProfileTransactionOwner;
    std::shared_ptr<ProfileTransactionOwner> profile_transaction_;
    Status begin_profile_transaction(bool grant,const StableId&,Integer,Integer,const ImportHistoryChoice*,const std::vector<EntryFingerprint>* legacy_selection=nullptr);
    Status begin_profile_merge_candidate(ProfileMergeCandidate&&,const WorldIdentity&,const StableId&);
    Status advance_metadata();
    void release_profile_transaction_inputs()noexcept;
    Bytes frozen_bytes_;
    CheckpointJobBytes frozen_job_bytes_;
    std::shared_ptr<CheckpointJobBudget> metadata_job_;
    std::shared_ptr<CheckpointMemoryResource> metadata_resource_;
    CheckpointMemoryCharge metadata_transport_memory_;
    const Bytes& frozen_profile_bytes()const noexcept{return metadata_job_?frozen_job_bytes_.bytes():frozen_bytes_;}
    std::optional<WorldIdentity> pending_source_;
    StableId pending_admission_id_;
    CheckpointMemoryCharge active_capsule_memory_;
    std::optional<SlotAdmissionCapsule> active_capsule_;
    std::optional<JobAdmissionCapsule> active_job_capsule_;
    const SlotAdmissionCapsule* active_capsule()const noexcept{return active_job_capsule_?&active_job_capsule_->value():(active_capsule_?&*active_capsule_:nullptr);}
    std::optional<WorldIdentity> active_identity_;
    struct PendingImport;
    std::shared_ptr<PendingImport> pending_import_;
    Status install_import_candidate(bool confirmed_media_failure);
    bool import_analysis_pending()const noexcept;
    Status continue_import_admission_write();
    struct PendingCreation;
    std::shared_ptr<PendingCreation> pending_creation_;
    Status start_creation_proof();
    Status advance_creation_proof();
    bool owner_thread()const noexcept;
    Status begin_pending_write();
    Status block(const StorageDiagnostic&);
    Status complete_write(const StoragePoll&);
    Status begin_metadata_candidate(ProfileMetadataSnapshot&&,const WorldIdentity&,const StableId& admission_id);
};
static_assert(std::is_nothrow_copy_constructible_v<ProfileAdmissionStatus>,"storage poll must not allocate another diagnostic");
}
