// 流式载入只构造独占候选；此类型不能从外部seed、shared const状态或成功标志伪造。
#pragma once
#include "decoded_world_seed.hpp"
#include "continuation_binding.hpp"
#include "profile_delivery_capture.hpp"
#include <atomic>
namespace hanmo::resident {
class ResidentWorld;
// 最后取消与世界安装由一个CAS排序；安装已开始后旧取消不能把真实成功改成未发生。
class WorldInstallCancellation {
public:
    bool request()noexcept{State expected=State::open;return state_.compare_exchange_strong(expected,State::cancelled)||expected==State::cancelled;}
    bool cancelled()const noexcept{return state_.load()==State::cancelled;}
    bool installed()const noexcept{return state_.load()==State::installed;}
private:
    friend class ResidentWorld;
    enum class State{open,cancelled,installing,installed};std::atomic<State> state_{State::open};
    bool begin()noexcept{State expected=State::open;return state_.compare_exchange_strong(expected,State::installing);}
    void finish()noexcept{state_.store(State::installed);}
};
struct BoundContinuationSource {CheckpointReadSource* source=nullptr;std::uint64_t begin=0,length=0;CheckpointDigest sha256{};};
class WorldRestoreCandidate {
public:
    WorldRestoreCandidate()=default;
    WorldRestoreCandidate(const WorldRestoreCandidate&)=delete;
    WorldRestoreCandidate& operator=(const WorldRestoreCandidate&)=delete;
    WorldRestoreCandidate(WorldRestoreCandidate&&)noexcept=default;
    WorldRestoreCandidate& operator=(WorldRestoreCandidate&&)noexcept;
    const WorldIdentity& source_identity()const noexcept{return source_;}
    const CatalogIdentity* catalog_identity()const noexcept{return world_?&world_->catalog->identity:nullptr;}
    bool validated()const noexcept{return world_&&validated_;}
    // 较早世界可以暂缺baseline条目；同key一旦存在必须同原S4源指纹，不能只比较ID。
    CheckpointJobStatus verify_admission_baseline(const WorldId&,const std::vector<EntryFingerprint>&,const CapacityProfile&,CheckpointJobBudget&)const;
    CheckpointJobResult<ValidatedImportDeliveries> capture_import_deliveries(const CapacityProfile&,CheckpointJobBudget&)const;
    static CheckpointJobResult<WorldRestoreCandidate> prepare(CheckpointReadSource&,const CheckpointDigest& full_checkpoint_digest,std::shared_ptr<const PreparedCatalog>,const BoundContinuationSource&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
private:
    friend class ResidentWorld;friend class NativeBoundarySession;
    CheckpointJobReservation memory_,continuation_memory_;
    // 只有prepare创建；shared控制为最终publish预备，外部没有候选可写别名。
    std::shared_ptr<WorldBundle> world_;
    CheckpointUniquePtr<ContinuationSnapshot> continuation_;
    WorldIdentity source_{};
    bool validated_=false,consumed_=false;
};
// 各域原JSON writer的同字段schema sink，只检查字段顺序/可导出形状，不构造BoundaryValue树。
// 由唯一world组合层分派实际选中provider；缺任何域不允许产生可安装候选。
Status validate_world_export_shape(const WorldBundle&,const CapacityProfile&,CheckpointJobBudget&);
}
