// 原ResidentWorld稳定边界产生的封闭只读租约；三载荷共用一次世界身份和原续行。
#pragma once
#include "continuation_capture.hpp"
#include "resident/checkpoint/checkpoint_domain_list.hpp"
#include <memory>
namespace hanmo::resident {
class ResidentWorld;
struct WorldBundle;
// 工厂钥匙只在原世界入口可构造；不能以外来shared<const>自报已冻结。
class WorldCheckpointCapture final:public CheckpointSnapshotLease,public std::enable_shared_from_this<WorldCheckpointCapture> {
public:
    class ConstructionKey {friend class ResidentWorld;ConstructionKey()=default;};
    WorldCheckpointCapture(ConstructionKey,CheckpointJobReservation&& retained,CheckpointMemoryCharge&& metadata,
        std::shared_ptr<CheckpointMemoryResource>,std::shared_ptr<const WorldBundle>,CheckpointIdentity&&,WorldIdentity,CheckpointDomainList&&,CapturedContinuation&&);
    WorldCheckpointCapture(const WorldCheckpointCapture&)=delete;
    WorldCheckpointCapture& operator=(const WorldCheckpointCapture&)=delete;
    const CheckpointIdentity& identity()const override{return identity_;}
    const WorldIdentity& source_identity()const noexcept{return source_;}
    const std::shared_ptr<const PreparedCatalog>& catalog()const noexcept;
    bool belongs_to(const CheckpointJobBudget& job)const noexcept{return retained_.belongs_to(job);}
    // 并行推进必须持整个capture租约到候选发布/失败回传结束，不能只拿resource后释放旧world账。
    const std::shared_ptr<CheckpointMemoryResource>& candidate_memory()const noexcept{return candidate_memory_;}
    CheckpointJobFailureLease failure_lease()const noexcept{return retained_.failure_lease();}
    // 只写同一真实typed值；section/版本由原catalog selector事先固定。
    Status visit_section(const StableId&,CodecVisitor&)const override;
    CheckpointJobResult<CheckpointStreamReceipt> write(CheckpointWriteSink&,const CapacityProfile&,CheckpointJobBudget&)const;
    // 不允许外部换source、换continuation或省略原捕获到的续行。
    CheckpointJobResult<CheckpointJobBytes> bind_continuation(const CheckpointDigest&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&)const;
private:
    CheckpointJobReservation retained_;
    CheckpointMemoryCharge metadata_;
    std::shared_ptr<CheckpointMemoryResource> candidate_memory_;
    std::shared_ptr<const WorldBundle> world_;
    CheckpointIdentity identity_;
    WorldIdentity source_;
    CheckpointDomainList domains_;
    CapturedContinuation continuation_;
};
// 以下两口仅复用world_codec原分派，不另造域版本或字段表。
CheckpointDomainList select_world_checkpoint_domains(const PreparedCatalog&);
Status visit_captured_world_checkpoint_section(const WorldBundle&,const StableId&,CodecVisitor&);
// 完整持有图含未选空表、派生缓存及原目录；调用在同一world线程、增加lease之前。
Status retain_world_checkpoint_graph(const WorldBundle&,CheckpointRetentionCensus&);
}
