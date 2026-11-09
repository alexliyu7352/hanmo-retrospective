// 原看管关闭的同 World 拥有者：低层位置、完整权限与 S4 原源分工，不复制持久账。
#pragma once
#include "named_military_custody_deadline.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/domains/actor_foundation/named_custody_availability_sources.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
struct NativeCompleteWorldCoreSources;
struct PoliticalTakeoverContext;
// 全部指针借本次 World scope；构造不获取 Complete lease，不跨候选缓存 clock/body。
struct NativeNamedMilitaryCustodyDependencies {
  NamedMilitaryCustodyContext custody;
  const SpatialCarrierSources &spatial;
  ActorBattleSpatialSources battle;
  const PoliticalTakeoverContext *takeover = nullptr;
  const MilitarySpatialHandoffState *military_handoffs = nullptr;
  const BirthPlacementRead *births = nullptr;
  const MilitaryActionPlacementRead *military_actions = nullptr;
  const FieldBattleExitPlacementRead *field_exits = nullptr;
  ExactArithmeticBudget budget;
};
class NativeNamedMilitaryCustodyWorldRead;
// 未选返回 null 只表示真实原选择未启用；已选缺根显式失败，绝不返回空实现。
Outcome<std::unique_ptr<NativeNamedMilitaryCustodyWorldRead>>
make_native_named_military_custody_world_read(
    const WorldBundle &, const NativeCompleteWorldCoreSources &, NativeNamedMilitaryCustodyDependencies);
// 地址固定：内部 context 引用自己的 placement/full-release，禁止移动造成悬挂。
class NativeNamedMilitaryCustodyWorldRead final : public NamedMilitaryCustodyReleaseRead,
                                                public CheckpointAllocatedObject {
  const WorldBundle &world_;
  NamedMilitaryCustodyTerritorySources territory_;
  OriginalNamedMilitaryCustodyPlacementRead placements_;
  NamedMilitaryCustodyReleaseContext sources_;
  OriginalNamedMilitaryCustodyReleaseRead releases_;
  NativeNamedCustodyAvailabilityRead availability_;
  NativeNamedMilitaryCustodyWorldRead(const WorldBundle &, NativeNamedMilitaryCustodyDependencies);
  friend Outcome<std::unique_ptr<NativeNamedMilitaryCustodyWorldRead>>
  make_native_named_military_custody_world_read(const WorldBundle &, const NativeCompleteWorldCoreSources &, NativeNamedMilitaryCustodyDependencies);
public:
  NativeNamedMilitaryCustodyWorldRead(const NativeNamedMilitaryCustodyWorldRead &) = delete;
  NativeNamedMilitaryCustodyWorldRead &operator=(const NativeNamedMilitaryCustodyWorldRead &) = delete;
  NativeNamedMilitaryCustodyWorldRead(NativeNamedMilitaryCustodyWorldRead &&) = delete;
  NativeNamedMilitaryCustodyWorldRead &operator=(NativeNamedMilitaryCustodyWorldRead &&) = delete;
  bool bound_to(const CommonStore &, const MilitaryState &) const noexcept override;
  Outcome<const NamedMilitaryCustodyRelease *> read_current_named_custody_release(const StableId &) const override;
  Status validate_sources() const;
  Outcome<std::vector<const NamedMilitaryCustodyTerm *>> read_due_terms(
      const WorldMoment &actual_from, const WorldMoment &requested_to) const;
  // S1 只借低层 placement，不能由它调用完整 release，避免历史重放回边。
  const NamedMilitaryCustodyPlacementRead &placements() const noexcept;
  const NativeNamedCustodyAvailabilityRead &availability() const noexcept;
  // 原 expiry writer 用此真实 context，加原 Body/read-stamp/capture 事务输入。
  const NamedMilitaryCustodyReleaseContext &sources() const noexcept;
};
} // namespace hanmo::resident
