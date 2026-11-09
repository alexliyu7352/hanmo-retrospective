// 已提交S0投影→S14独立集合事务；准入history权限仅来自本机record。
#pragma once
#include "resident_profile_store.hpp"
#include "resident/kernel/profile_delivery_capture.hpp"
namespace hanmo::resident::storage {
struct ProfileMergeCandidate {CheckpointMemoryCharge result_memory;ProfileMetadataSnapshot snapshot;std::vector<StableId> accepted_delivery_ids,already_present_ids,historical_quarantined_ids;};
Outcome<ProfileMergeCandidate> merge_profile_deliveries(const ProfileMetadataSnapshot&,const StableId& admission_id,Integer expected_generation,const CommittedProfileDeliveries&,const ProfileStoreLimits&);
Outcome<ProfileMergeCandidate> merge_profile_deliveries(const ProfileMetadataSnapshot&,const StableId& admission_id,Integer expected_record_generation,Integer expected_profile_generation,const CommittedProfileDeliveries&,const ProfileStoreLimits&);
// 显式分kind选择、真实已提交来源和record/profile双CAS；权限与集合/ack共同一代。
Outcome<ProfileMergeCandidate> grant_imported_history(const ProfileMetadataSnapshot&,const StableId& admission_id,Integer expected_record_generation,Integer expected_profile_generation,const std::vector<EntryFingerprint>& cards,const std::vector<EntryFingerprint>& unlocks,const CommittedProfileDeliveries&,const ProfileStoreLimits&);
// 导入新关系的baseline始终完整从已验证快照取；外来capsule不能授allowed_history。
Outcome<AdmissionRecord> create_import_admission(const SlotAdmissionCapsule& chosen_host_capsule,const std::vector<EntryFingerprint>& validated_snapshot_outbox,const AdmissionCodecLimits&);
// 用户明确选择的kind/mode历史集合才并入本机allowed_history；原baseline不可改。
Outcome<AdmissionRecord> authorize_admission_history(const AdmissionRecord&,const std::vector<EntryFingerprint>& requested,const AdmissionCodecLimits&);
Status validate_profile_metadata_complete(const ProfileMetadataSnapshot&,const ProfileStoreLimits&);
Outcome<Bytes> encode_profile_metadata_complete(const ProfileMetadataSnapshot&,const ProfileStoreLimits&);
ProfileMetadataDecoded decode_profile_metadata_complete(const Bytes&,const ProfileStoreLimits&);
CheckpointJobResult<CheckpointJobBytes> encode_profile_metadata_complete(const ProfileMetadataSnapshot&,const ProfileStoreLimits&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
CheckpointJobResult<ProfileMetadataSnapshot> decode_profile_metadata_complete(const CheckpointJobBytes&,const ProfileStoreLimits&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
CheckpointJobResult<ProfileMetadataSelected> select_profile_metadata(const JobSlotBankPair&,const ProfileNamespace&,const ProfileStoreLimits&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
}
