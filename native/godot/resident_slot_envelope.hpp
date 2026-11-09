// 槽/2仅组合原checkpoint、真实续行和准入胶囊；物理完整性不代替世界/本机权限核验。
#pragma once
#include "resident/checkpoint/checkpoint_envelope.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident_slot_media.hpp"
#include <array>

namespace hanmo::resident::storage {
// 固定三个目录项；缺续行必须全零，不允许第四种载荷或同号猜格式。
struct SlotPayloadDescriptor {
    std::uint8_t kind=0,present=0;
    std::uint16_t codec=0;
    std::uint64_t offset=0,length=0;
    CheckpointDigest sha256{};
};
struct SlotEnvelopeIndex {
    StableId save_id;
    Integer generation=0;
    std::uint64_t saved_at_unix_seconds=0,total_file_bytes=0;
    std::array<SlotPayloadDescriptor,3> payloads{};
    CheckpointDigest envelope_sha256{};
};
// 目录字符串与对象归同job保留；销毁实际对象后才退其预留。
class SlotEnvelopeInspection {
public:
    SlotEnvelopeInspection()=default;
    SlotEnvelopeInspection(const SlotEnvelopeInspection&)=delete;
    SlotEnvelopeInspection& operator=(const SlotEnvelopeInspection&)=delete;
    SlotEnvelopeInspection(SlotEnvelopeInspection&&) noexcept=default;
    SlotEnvelopeInspection& operator=(SlotEnvelopeInspection&&) noexcept;
    const SlotEnvelopeIndex& value()const noexcept{return value_;}
private:
    friend struct SlotEnvelopeAccess;
    CheckpointJobReservation reservation_;
    SlotEnvelopeIndex value_;
};
// 核心捕获者实现此只读replay源；两遍读同一拥有snapshot，不捕获可变session/world裸引用。
class SlotCheckpointProducer {
public:
    virtual ~SlotCheckpointProducer()=default;
    virtual const CheckpointIdentity& identity()const noexcept=0;
    virtual std::uint16_t codec_version()const noexcept=0;
    virtual CheckpointJobResult<CheckpointStreamReceipt> write(CheckpointWriteSink&,CheckpointJobBudget&)const=0;
};
// 一次保存永久冻结时间/代数/摘要/三载荷；重试只重放本对象，不能从新世界生成同代内容。
class FrozenSlotEnvelope : public CheckpointAllocatedObject {
public:
    FrozenSlotEnvelope(const FrozenSlotEnvelope&)=delete;
    FrozenSlotEnvelope& operator=(const FrozenSlotEnvelope&)=delete;
    const SlotEnvelopeIndex& index()const noexcept{return index_;}
    bool belongs_to(const CheckpointJobBudget& job)const noexcept{return control_.belongs_to(job);}
    const CheckpointIdentity& checkpoint_identity()const noexcept{return checkpoint_->identity();}
    // sink由原worker提供；函数不接世界、SceneTree或新的时间来源。
    CheckpointJobStatus write(CheckpointWriteSink&,CheckpointJobBudget&)const;
private:
    friend struct SlotEnvelopeAccess;
    FrozenSlotEnvelope()=default;
    CheckpointJobReservation control_;
    SlotEnvelopeIndex index_;
    std::shared_ptr<const SlotCheckpointProducer> checkpoint_;
    CheckpointJobBytes header_,continuation_,admission_;
    std::uint64_t chunk_bytes_=0;
};
// 只在已实际捕获的原core producer、同job HMCB/胶囊字节之间组合，不开放save成功位。
CheckpointJobResult<std::shared_ptr<const FrozenSlotEnvelope>> freeze_slot_envelope(
    const StableId& save_id,Integer generation,std::uint64_t saved_at_unix_seconds,
    std::shared_ptr<const SlotCheckpointProducer>,CheckpointJobBytes&& continuation,
    CheckpointJobBytes&& admission,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// 有界实际读取按安全头→全档摘要→目录/逐载荷摘要核验，并对真正解释字节再做总摘要防ABA。
CheckpointJobResult<SlotEnvelopeInspection> inspect_slot_envelope(
    CheckpointReadSource&,const StableId& expected_save_id,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// 显式外来导入无本机槽名；仍核原合法save_id，但文件名不授本机槽身份。
CheckpointJobResult<SlotEnvelopeInspection> inspect_import_slot_envelope(CheckpointReadSource&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// 只把实际短读/摘要错列为可考虑另一银行；预算、未知版本、I/O和内容错保首错停止。
enum class SlotBankCorruption { none,truncated,digest,payload_digest };
struct SlotEnvelopeBankScan {SlotBankCorruption corruption=SlotBankCorruption::none;SlotEnvelopeInspection inspection;};
struct SelectedSlotEnvelope {
    bool present=false;SlotBank bank=SlotBank::a;SlotEnvelopeInspection inspection;
    SlotBankCorruption a_corruption=SlotBankCorruption::none,b_corruption=SlotBankCorruption::none;
};
CheckpointJobResult<SlotEnvelopeBankScan> scan_slot_envelope_bank(CheckpointReadSource&,const StableId&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
CheckpointJobResult<SelectedSlotEnvelope> select_slot_envelope_banks(const SlotStreamBankPair&,const StableId&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// 已验目录的有限视图不复制大档；父源寿命由原stage/file lease拥有者保持。
class SlotPayloadReadSource final:public CheckpointReadSource {
public:
    SlotPayloadReadSource(CheckpointReadSource& source,const SlotPayloadDescriptor& descriptor)
        :source_(source),descriptor_(descriptor){}
    std::uint64_t size()const override{return descriptor_.length;}
    Status read(std::uint64_t,std::uint8_t*,std::size_t)override;
private:
    CheckpointReadSource& source_;
    SlotPayloadDescriptor descriptor_;
};
} // stored receipt只由原coordinator在语义读回和真实durable终态后生成。
