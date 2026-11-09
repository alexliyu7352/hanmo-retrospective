// 青川 Field 的同 World 原军编组图；只拥有瞬态 reader，不创建第二份运输账。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <memory>
#include <vector>
namespace hanmo::resident {
struct WorldBundle;
struct NativeCompleteWorldCoreSources;
struct DynamicStepInput;
struct GroupMovementContext;
struct GroupMilitarySupplyRebindSource;
struct ExactPeriodContext;
class NativeFieldWorldScope;
class CurrentReadStampRead;
class NativeTransportInventoryEntryRead;
class TransportManifestHistorySourceRead;
// core 和 World 由同一不移动 Complete owner 保活；每个实际 M 单独构造。
class NativeFieldGroupWorld final : public CheckpointAllocatedObject {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit NativeFieldGroupWorld(std::unique_ptr<Impl>);

public:
  ~NativeFieldGroupWorld();
  NativeFieldGroupWorld(const NativeFieldGroupWorld &) = delete;
  NativeFieldGroupWorld &operator=(const NativeFieldGroupWorld &) = delete;
  static Outcome<std::unique_ptr<NativeFieldGroupWorld>>
  create(const WorldBundle &, const NativeCompleteWorldCoreSources &,
         WorldBundle *actual_candidate = nullptr,
         const DynamicStepInput *actual_phase = nullptr);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  // 完整冷门另行调用；构造不抢在 manifest 派生值恢复之前验当前数量。
  Status validate() const;
  const GroupMovementContext &group() const noexcept;
  NativeFieldWorldScope &field() const noexcept;
  const CurrentReadStampRead &stamps() const noexcept;
  const NativeTransportInventoryEntryRead &inventory_entries() const noexcept;
  const TransportManifestHistorySourceRead &manifests() const noexcept;
};
// 原 Opening 前后分别构造本 World 短读域；返回的准备值不借票、不借临时 owner。
Outcome<std::vector<GroupMilitarySupplyRebindSource>> prepare_native_field_supply_movements(
    const WorldBundle&,const ExactPeriodContext&);
Status apply_native_field_supply_movements(WorldBundle&,const ExactPeriodContext&,
    const std::vector<GroupMilitarySupplyRebindSource>&);
} // namespace hanmo::resident
