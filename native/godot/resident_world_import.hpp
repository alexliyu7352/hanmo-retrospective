// 外来slot/2只经完整私有冷核产生预览；用户选择与原字节/来源/namespace绑定。
#pragma once
#include "resident_profile_merge.hpp"
#include "resident_storage_diagnostic.hpp"
#include "resident/kernel/world_restore_candidate.hpp"
namespace hanmo::resident::storage {
struct WorldImportPreviewIdentity {std::uint64_t instance=0,operation=0;CheckpointDigest source_bytes_sha256{};WorldIdentity snapshot;};
struct ImportHistoryChoice {std::vector<EntryFingerprint> card_history,actor_unlock_history;};
enum class WorldImportPhase {preparing,preview_ready,revalidating,preparing_admission,persisting_admission,rereading_admission,awaiting_recovery,awaiting_failed_media_confirmation,ready_to_install,installed,cancelling,cancelled,failed};
// 返回只读共享拥有值；主线程不接fd，预览也没有world安装能力。
class WorldImportPreview {
public:
    const WorldIdentity& snapshot_identity()const noexcept{return projection_.source();}
    const CheckpointDigest& source_bytes_sha256()const noexcept{return source_sha256_;}
    const std::vector<EntryFingerprint>& baseline()const noexcept{return baseline_;}
    const std::vector<ResolvedProfileDelivery>& history()const noexcept{return projection_.entries();}
private:
    friend class ResidentProfileCoordinator;friend class WorldImportAnalysis;
    CheckpointMemoryCharge memory_;
    ValidatedImportDeliveries projection_;
    std::vector<EntryFingerprint> baseline_;
    CheckpointDigest source_sha256_{},checkpoint_sha256_{};
};
struct WorldImportStatus {WorldImportPhase phase=WorldImportPhase::preparing;WorldImportPreviewIdentity identity;bool world_committed=false,admission_stored=false,profile_sync_blocked=true;std::shared_ptr<const WorldImportPreview> preview;CheckpointJobFailureLease failure;StorageDiagnostic code;};
// 原完整来源在此只授用户明确选择的baseline成员；记录和META集合是一代事务。
Outcome<ProfileMergeCandidate> resume_import_admission(const ProfileMetadataSnapshot&,const AdmissionRecord&,Integer expected_generation,const ValidatedImportDeliveries&,const ProfileStoreLimits&);
Outcome<ProfileMergeCandidate> merge_import_admission(const ProfileMetadataSnapshot&,const AdmissionRecord&,Integer expected_generation,const ValidatedImportDeliveries&,const ProfileStoreLimits&);
static_assert(std::is_nothrow_copy_constructible_v<WorldImportStatus>,"storage poll must not allocate another diagnostic");
}
