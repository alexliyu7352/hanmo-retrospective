// S0唯一outbox只接受S4实际取得闭合引用，在原成功候选内确定分配ID；冷读不补写。
#pragma once
#include "resident/domains/growth/card_acquisition_contract.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
namespace hanmo::resident {
// S0指纹身份由真实条目及完整S4来源决定；S14/HMAC共用此值，不重定义近似key。
struct DeliveryKey {WorldId source_world_id{};StableId delivery_id;};
struct EntryFingerprint {DeliveryKey key;CheckpointDigest entry_sha256{};};
enum class ProfileDeliveryKind:std::uint8_t{card_discovery=0,actor_unlock=1};
struct ProfileDeliveryEntry {
    StableId delivery_id;WorldId source_world_id{};Integer source_tick=0;
    CharacterCreationMode mode=CharacterCreationMode::standard;
    ProfileDeliveryKind kind=ProfileDeliveryKind::card_discovery;StableId subject_id;
    ProfileAcquisitionSource acquisition_source;
};
struct ProfileDeliveryOutbox {std::vector<ProfileDeliveryEntry> entries;};
struct ProfileDeliveryCursor {WorldIdentity source;std::uint64_t offset=0;};
struct PendingProfileDeliveries {WorldIdentity source_identity;std::vector<ProfileDeliveryEntry> entries;std::optional<ProfileDeliveryCursor> cursor;};
Outcome<Integer> profile_delivery_selected_version(const CatalogIdentity&);
// 每kind的standard/free主体并集最多4096；同取得只并到原条目，不产生无界逐次日志。
Status append_profile_acquisition_deliveries(CommonStore&,ProfileDeliveryOutbox&,const CardAcquisitionSources&,CardAcquisitionState&,const ProfileControlEligibilitySources&,const ProfileAcquisitionSource&);
Status validate_profile_delivery_outbox(const CommonState&,const ProfileDeliveryOutbox&,const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileControlEligibilitySources&);
Outcome<PendingProfileDeliveries> read_pending_profile_deliveries(
    const WorldIdentity &, const ProfileDeliveryOutbox &,
    std::optional<ProfileDeliveryCursor> = {}, Integer limit = 32);
Status visit_profile_delivery_outbox(CodecVisitor&,ProfileDeliveryOutbox&);
class ProjectionWriter;
// 原V§12.4唯一entries根；JSON、无树shape与binary共用同七字段。
// 这里只核物理形状/原有限唯一性，实际S4/控制来源仍须原完整world validator。
Status emit_profile_delivery_outbox_fields(ProjectionWriter&,const ProfileDeliveryOutbox&);
Outcome<BoundaryValue> encode_profile_delivery_outbox(const ProfileDeliveryOutbox&);
Outcome<ProfileDeliveryOutbox> decode_profile_delivery_outbox(const BoundaryValue&);
// 完整fingerprint含解析后的S4实际来源；不以裸reference ID代历史payload。
CheckpointJobResult<CheckpointDigest> profile_delivery_fingerprint(const ProfileDeliveryEntry&,const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileControlEligibilitySources&,const CapacityProfile&,CheckpointJobBudget&);
} // worker只消费已提交投影，世界不因profile I/O时序改变tick/RNG/revision。
