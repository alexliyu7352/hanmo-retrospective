// 同实际 M 的独立 LE FIFO 来源图，不反向持有 Household 或其父 domain owner。
#pragma once
#include "economic_fifo_read.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/local_economy/economic_facility_installed_sources.hpp"
#include "resident/domains/local_economy/production/production_input_lease.hpp"
#include "resident/domains/politics/political_exact_phase.hpp"
#include "resident/kernel/native_complete_world_core.hpp"
namespace hanmo::resident {
struct NativeEconomicFifoWorldLateSources {
  // 只能 alias 真 S9 原来源 owner；I/L 未选才可空，不接受空成功替身。
  std::shared_ptr<const ItemOutputSourceRead> item_outputs;
  // 前序 P1 只借原 P01 状态；本 scope 随后真正构造生产 InputLease。
  // 下列 alias 只能保活独立前序，禁止 shared 回持本 scope 的父 Domain owner。
  std::shared_ptr<const PoliticalExactReadLease> political;
  std::shared_ptr<const NativeEconomicInstalledFacilityAppliedRead> installed;
  std::shared_ptr<const ProductionFacilityEffectRead> condition_effects;
  std::shared_ptr<const PoliticalTakeoverContext> takeover;
  std::shared_ptr<const PopulationTransportSourceRead> transport;
  const PopulationMilitaryReadContext *population_military = nullptr;
  // 只保活原付款来源；缺席不授予完整 EconomicExact 付款执行能力。
  std::shared_ptr<const EconomicOriginalPaymentRead> original_payments;
};
class NativeEconomicFifoWorldScope final : public CheckpointAllocatedObject {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit NativeEconomicFifoWorldScope(std::unique_ptr<Impl>);

public:
  ~NativeEconomicFifoWorldScope();
  NativeEconomicFifoWorldScope(const NativeEconomicFifoWorldScope &) = delete;
  NativeEconomicFifoWorldScope &
  operator=(const NativeEconomicFifoWorldScope &) = delete;
  NativeEconomicFifoWorldScope(NativeEconomicFifoWorldScope &&) = delete;
  NativeEconomicFifoWorldScope &
  operator=(NativeEconomicFifoWorldScope &&) = delete;
  // core/World/原 phase 由同 job 更早的 owner 保活；构造不读取 holder
  // 或完整工厂。
  static Outcome<std::unique_ptr<NativeEconomicFifoWorldScope>>
  create(const NativeCompleteWorldCoreSources &,
         NativeEconomicFifoWorldLateSources = {},
         const DynamicStepInput *actual_phase = nullptr,
         EconomicWorkSourcesState *mutable_production = nullptr);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  const NativeEconomicMaterialRead &materials() const noexcept;
  const NativeEconomicFifoRead &fifo() const noexcept;
  const NativeEconomicProductionInputLease *production_inputs() const noexcept;
  EconomicProductionContext *production() const noexcept;
};
} // namespace hanmo::resident
