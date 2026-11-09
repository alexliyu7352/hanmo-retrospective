// 原预算批准及条约真实边界的公库前像；只借同一 Complete
// 原根，不产生第二份产权账。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/politics/budget/political_budget_policy_read.hpp"
#include "resident/domains/politics/treaty/treaty_aid_accepted_resource_read2.hpp"
#include <memory>
namespace hanmo::resident {
struct NativeCompleteWorldCoreSources;
struct DynamicStepInput;
// 固定地址拥有 Naval 原历史所需上下文；外层必须保活 core 与实际 phase。
class NativePublicAssetHistoryRead final
    : public PublicBudgetTreasuryHistoryRead,
      public TreatyAidAcceptedPublicAssetHistoryRead,
      public CheckpointAllocatedObject {
  struct Sources;
  std::unique_ptr<Sources> sources_;
  explicit NativePublicAssetHistoryRead(std::unique_ptr<Sources>);

public:
  ~NativePublicAssetHistoryRead();
  NativePublicAssetHistoryRead(const NativePublicAssetHistoryRead &) = delete;
  NativePublicAssetHistoryRead &
  operator=(const NativePublicAssetHistoryRead &) = delete;
  NativePublicAssetHistoryRead(NativePublicAssetHistoryRead &&) = delete;
  NativePublicAssetHistoryRead &
  operator=(NativePublicAssetHistoryRead &&) = delete;
  static Outcome<std::unique_ptr<NativePublicAssetHistoryRead>>
  create(const NativeCompleteWorldCoreSources &, const PoliticalBudgetState &,
         const DynamicStepInput *actual_phase = nullptr);
  // 原条约 owning graph 先于本 lease 存活；不要求借不存在的预算批准来取得切口。
  static Outcome<std::unique_ptr<NativePublicAssetHistoryRead>>
  create_for_treaty(const NativeCompleteWorldCoreSources &,
                    const TreatyResourceSourceLinks2 &,
                    const DynamicStepInput *actual_phase = nullptr);
  bool bound_to(const WorldBundle &) const noexcept override;
  bool bound_to(const CommonStore &) const noexcept override;
  // 只验真根/当前钟绑定；每次业务读取仍重新核实际批准、原产权全部边及前像。
  Status validate_bindings() const;
  Outcome<Owner> read_public_budget_asset_owner_before_approval(
      const PoliticalBudgetState &, const BudgetAuthorizationSource &,
      const ResourceKey &) const override;
  Outcome<Owner>
  read_current_owner_for_ratification(const TreatyResourceSourceLinks2 &,
                                      const TreatyAidResourceProposalSource2 &,
                                      const ResourceKey &,
                                      const WorldMoment &) const override;
  Outcome<Owner>
  read_owner_before_signature(const TreatyResourceSourceLinks2 &,
                              const TreatyResourceSignatureSource2 &,
                              const ResourceKey &) const override;
  Outcome<Owner>
  read_owner_before_aid_acceptance(const TreatyResourceSourceLinks2 &,
                                   const TreatyAidAcceptedResourceSource2 &,
                                   const ResourceKey &) const override;
};
} // namespace hanmo::resident
