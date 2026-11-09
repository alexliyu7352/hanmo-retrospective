// 本机元档案显式版本；准入、四META分区和ack复用同一真实A/B代。
#pragma once
#include "resident_profile_admission.hpp"
#include "resident_slot_media.hpp"
#include "resident/domains/growth/card_acquisition_contract.hpp"

namespace hanmo::resident::storage {
// 这一个namespace的准入/空集合/ack共享一代；旧GD profile文件从不读取或覆盖。
struct ProfileMetadataSnapshot {
    ProfileMetadataSnapshot()=default;
    ProfileMetadataSnapshot(const ProfileMetadataSnapshot&);
    ProfileMetadataSnapshot& operator=(const ProfileMetadataSnapshot&);
    ProfileMetadataSnapshot(ProfileMetadataSnapshot&&)noexcept=default;
    ProfileMetadataSnapshot& operator=(ProfileMetadataSnapshot&&)noexcept;
    CheckpointMemoryCharge owned_memory;
    CheckpointJobReservation decoded_memory;
    ProfileNamespace name_space;
    Integer generation=0;
    std::uint16_t wire_version=1;
    std::vector<AdmissionRecord> records;
    // 普通/free、发现/合法解锁分区；人物解锁保存真实完整S4来源供跨局冷核。
    std::vector<StableId> standard_cards,free_cards;
    std::vector<ResolvedProfileAcquisition> standard_actors,free_actors;
    std::vector<EntryFingerprint> acknowledgements;
};
struct ProfileStoreLimits {
    AdmissionCodecLimits admission;
    std::size_t max_records=0;
    std::uint64_t max_file_bytes=0;
};
struct ProfileMetadataDecoded {
    bool ok=false,physical_corruption=false;
    StableId code;
    ProfileMetadataSnapshot snapshot;
};
struct ProfileMetadataSelected {
    bool ok=false,not_found=false,recovered=false;
    StableId code;
    SlotBank bank=SlotBank::b;
    ProfileMetadataSnapshot snapshot;
};
// /1沿原空成果布局；完整消费者显式选/2，不能根据字段是否为空猜版本。
Outcome<Bytes> encode_profile_metadata(const ProfileMetadataSnapshot&,const ProfileStoreLimits&);
ProfileMetadataDecoded decode_profile_metadata(const Bytes&,const ProfileStoreLimits&);
ProfileMetadataSelected select_profile_metadata(const SlotBankPair&,const ProfileNamespace&,const ProfileStoreLimits&);
const AdmissionRecord* find_profile_admission(const ProfileMetadataSnapshot&,const StableId&) noexcept;
// 原record不存在时插入；完整相同记录为幂等，任何同world/ID换源均拒绝。
Outcome<ProfileMetadataSnapshot> insert_local_creation_record(const ProfileMetadataSnapshot&,const AdmissionRecord&,const ProfileStoreLimits&,std::uint16_t wire_version=1);
}
