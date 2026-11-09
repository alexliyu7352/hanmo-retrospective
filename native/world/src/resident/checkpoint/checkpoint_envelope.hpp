// 检查点外层只绑定世界/目录身份和各域字节；域字段与跨域引用由owner和中央校验。
#pragma once
#include "codec_visitor.hpp"
#include "checkpoint_stream.hpp"
#include "checkpoint_capacity.hpp"
#include <functional>

namespace hanmo::resident {
// epoch和run许可不持久化；中央成功恢复发布后分配新的执行epoch。
struct CheckpointIdentity { WorldId world_id{}; Integer revision=0,tick=0; CatalogIdentity catalog; };
struct CheckpointSection { StableId domain; Integer codec_version=1; CodecVisitor::Buffer payload; };
struct CheckpointEnvelope { CheckpointIdentity identity; std::vector<CheckpointSection> sections; std::uint32_t container_version=1; };
// 显式期待域顺序/版本阻止遗漏域、重复域和未知域悄悄通过；不定义域schema。
struct CheckpointDomain { StableId name; Integer codec_version=1; };
Outcome<CodecVisitor::Buffer> encode_checkpoint(const CheckpointEnvelope&);
// 冷选择只验封装摘要与已知域版本；不能作为完整世界语义准入凭证。
Outcome<CheckpointEnvelope> inspect_checkpoint(const CodecVisitor::Buffer&,const std::vector<CheckpointDomain>&);
// 多个历史合同共用一次完整封装校验；仍要求命中原有某一完整域顺序/版本。
Outcome<CheckpointEnvelope> inspect_checkpoint_variants(const CodecVisitor::Buffer&,const std::vector<std::vector<CheckpointDomain>>&);
Outcome<CheckpointEnvelope> decode_checkpoint(const CodecVisitor::Buffer&,const CatalogIdentity&,const std::vector<CheckpointDomain>&);
// 具名owner独占冻结typed快照；实现者不得持live世界裸引用或对外暴露可写别名。
class CheckpointSnapshotLease {
public:
    virtual ~CheckpointSnapshotLease()=default;
    virtual const CheckpointIdentity& identity() const=0;
    virtual Status visit_section(const StableId&,CodecVisitor&) const=0;
};
// 两遍只调用同一有所有权lease；所有段必须同owner，不能混拼不同快照。
struct CheckpointSectionProducer { StableId domain; Integer codec_version=1; std::shared_ptr<const CheckpointSnapshotLease> snapshot; };
// 物理目录只有范围和摘要；流式检查不复制任何整段payload。
struct CheckpointSectionIndex { StableId domain; Integer codec_version=1; std::uint64_t offset=0,bytes=0; CheckpointDigest sha256{}; };
struct CheckpointStreamIndex { CheckpointIdentity identity; std::vector<CheckpointSectionIndex> sections; CheckpointStreamReceipt receipt; };
// expected来自原core精确profile选择；编码器验证精确集合后以UTF8字节升序写目录。
Outcome<CheckpointStreamReceipt> encode_checkpoint_v2(const CheckpointIdentity&,const std::vector<CheckpointSectionProducer>&,const std::vector<CheckpointDomain>& expected,const CapacityProfile&,CheckpointWriteSink&);
// 小Bytes入口仍调用同一两遍流编码器；另有明确transport上限，不能冒充大档native job。
Outcome<CodecVisitor::Buffer> encode_checkpoint_v2_bytes(const CheckpointIdentity&,const std::vector<CheckpointSectionProducer>&,const std::vector<CheckpointDomain>& expected,const CapacityProfile&,std::uint64_t transport_byte_limit);
// 有界小档兼容解码明确保留所有payload；完整领域候选还须共享CodecDecodeBudget。
Outcome<CheckpointEnvelope> decode_checkpoint_v2_bytes(const CodecVisitor::Buffer&,const CatalogIdentity&,const std::vector<CheckpointDomain>&,const CapacityProfile&);
// 显式同job运输结果：预留先于真实对象声明，析构先销毁对象再退账。
class CheckpointJobResultAccess;
class CheckpointJobStreamIndex {
public:
    CheckpointJobStreamIndex() noexcept=default;
    CheckpointJobStreamIndex(const CheckpointJobStreamIndex&)=delete;
    CheckpointJobStreamIndex& operator=(const CheckpointJobStreamIndex&)=delete;
    CheckpointJobStreamIndex(CheckpointJobStreamIndex&&) noexcept=default;
    CheckpointJobStreamIndex& operator=(CheckpointJobStreamIndex&&) noexcept;
    const CheckpointStreamIndex& value()const noexcept{return value_;}
    bool belongs_to(const CheckpointJobBudget& j)const noexcept{return reservation_.belongs_to(j);}
private:
    friend class CheckpointJobResultAccess;
    CheckpointJobReservation reservation_;
    CheckpointStreamIndex value_;
};
class CheckpointJobEnvelope {
public:
    CheckpointJobEnvelope() noexcept=default;
    CheckpointJobEnvelope(const CheckpointJobEnvelope&)=delete;
    CheckpointJobEnvelope& operator=(const CheckpointJobEnvelope&)=delete;
    CheckpointJobEnvelope(CheckpointJobEnvelope&&) noexcept=default;
    CheckpointJobEnvelope& operator=(CheckpointJobEnvelope&&) noexcept;
    const CheckpointEnvelope& value()const noexcept{return value_;}
    bool belongs_to(const CheckpointJobBudget& j)const noexcept{return reservation_.belongs_to(j);}
private:
    friend class CheckpointJobResultAccess;
    CheckpointJobReservation reservation_;
    CheckpointEnvelope value_;
};
// 这些重载只覆盖本容器真实分配；lease clone、域visitor内部分配须由其owner接同一job。
CheckpointJobResult<CheckpointStreamReceipt> encode_checkpoint_v2(const CheckpointIdentity&,const std::vector<CheckpointSectionProducer>&,const std::vector<CheckpointDomain>&,const CapacityProfile&,CheckpointWriteSink&,CheckpointJobBudget&);
CheckpointJobResult<CheckpointJobBytes> encode_checkpoint_v2_bytes(const CheckpointIdentity&,const std::vector<CheckpointSectionProducer>&,const std::vector<CheckpointDomain>&,const CapacityProfile&,std::uint64_t transport_byte_limit,CheckpointJobBudget&);
CheckpointJobResult<CheckpointJobEnvelope> decode_checkpoint_v2_bytes(const CheckpointJobBytes&,const CatalogIdentity&,const std::vector<CheckpointDomain>&,const CapacityProfile&,CheckpointJobBudget&);
} // 物理槽原子替换属于P2，不在此模块伪造保存成功。
