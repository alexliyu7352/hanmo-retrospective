// StoredSlotReceipt/2只是实际介质回执；既非世界事实，也不授准入或提升权限。
#pragma once
#include "resident_slot_envelope.hpp"
namespace hanmo::resident::storage {
struct StoredSlotReceiptValue {
    StableId slot_id;Integer generation=0;CheckpointIdentity checkpoint_identity;
    CheckpointDigest payload_sha256{};std::uint64_t byte_length=0;
    // header UTC秒的明确显示输入；adapter格式化现实时间，不拿它判断业务Moment。
    std::uint64_t saved_at=0;StableId verification;
    CheckpointDigest admission_capsule_sha256{};
};
class StoredSlotReceipt {
public:
    explicit StoredSlotReceipt(CheckpointMemoryCharge&& charge,StoredSlotReceiptValue&& value):charge_(std::move(charge)),value_(std::move(value)){}
    const StoredSlotReceiptValue& value()const noexcept{return value_;}
private:CheckpointMemoryCharge charge_;StoredSlotReceiptValue value_;
};
}
