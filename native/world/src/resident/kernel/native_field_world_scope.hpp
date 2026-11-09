// 同一个实际 World/Complete owner 的原 Field 读图；不另造战场、身体或时钟。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
struct CompleteExactWorldReadBindings;
struct NativeCompleteWorldCoreSources;
struct GroupMovementContext;
struct CharacterConditionSources;
struct MilitaryMarchAuthorSelection;
class GroupContactMemberReleaseRead;
struct DynamicStepInput;
struct FieldBattleAbilityReadContext;
struct FieldExactContactReadContext;
struct FieldGroupHandoffReadContext;
struct PoliticalRelationsReadContext;
class FieldBattleExitPlacementRead;
class MilitaryFiniteReturnSourceRead;
class NativeCompleteWorldSources;
class NativeNamedCustodyAvailabilityRead;
// 外层必须使 World 和传入的 Complete owner 活到此对象析构；不允许跨 clone
// 复用。
class NativeFieldWorldScope final : public CheckpointAllocatedObject {
  friend class NativeCompleteWorldSources;
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit NativeFieldWorldScope(std::unique_ptr<Impl>);
  // 同一个最早 Field body 值快照只补原看管指针；不重建或刷新业务快照。
  Status bind_original_named_custody(
      const NativeNamedCustodyAvailabilityRead &);

public:
  ~NativeFieldWorldScope();
  NativeFieldWorldScope(const NativeFieldWorldScope &) = delete;
  NativeFieldWorldScope &operator=(const NativeFieldWorldScope &) = delete;
  static Outcome<std::unique_ptr<NativeFieldWorldScope>>
  create(const WorldBundle &, const CompleteExactWorldReadBindings &,
         const DynamicStepInput *actual_phase = nullptr);
  // 唯一私有 Complete 构造栈可借已构成 core，不回调尚未发布的 owner.reads()。
  static Outcome<std::unique_ptr<NativeFieldWorldScope>>
  create_from_original_core(const WorldBundle &,
                            const NativeCompleteWorldCoreSources &,
                            const DynamicStepInput *actual_phase = nullptr);
  // 只在同一词法 Group owner 构造期绑定一次，不改持久根或生成来源。
  Status bind_actual_group_context(const GroupMovementContext &);
  const CharacterConditionSources &body_sources() const noexcept;
  const MilitaryMarchAuthorSelection *march_selection() const noexcept;
  const GroupContactMemberReleaseRead *member_releases() const noexcept;
  bool bound_to(const WorldBundle &) const noexcept;
  const FieldBattleAbilityReadContext &abilities() const noexcept;
  const FieldExactContactReadContext *exact_contacts() const noexcept;
  const PoliticalRelationsReadContext *diplomacy() const noexcept;
  const FieldGroupHandoffReadContext *group_handoffs() const noexcept;
  const FieldBattleExitPlacementRead *placements() const noexcept;
  // 同 Impl 拥有的真实有限返程读口；未选 Group 时为 null。
  const MilitaryFiniteReturnSourceRead *finite_returns() const noexcept;
  // 原完整 Field validator 使用本图；不是只核表形状或当前人物投影。
  Status validate() const;
};
// 空历史仍走原反扫；真实已选完整图非空时借同 World 原 owner 核所有源。
Status validate_native_field_world_sources(const WorldBundle &);
} // namespace hanmo::resident
