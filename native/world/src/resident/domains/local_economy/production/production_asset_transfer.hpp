// 原 E01 私产报价采用；只有同候选真实写入与后继完成后才能由 S0 封口发布。
#pragma once
#include "facility_construction_adoption.hpp"
#include "production_asset_title_read.hpp"

namespace hanmo::resident {
// 只读结果借同 job 原字段，报价不预写 seller/buyer 同意或任何资源。
struct ProductionAssetTransferQuoteRead {
  ProductionAssetTitleRead title;
  ProductionAssetPrivateParties parties;
  const WorldIdentity *source_identity = nullptr;
  Integer price = 0;
};
Outcome<ProductionAssetTransferQuoteRead> quote_production_asset_transfer(
    const ProductionAssetLifecycleReadContext &, const StableId &seller_actor,
    const StableId &asset, const StableId &buyer_actor, Integer price);
struct ProductionAssetLifecycleWriteContext {
  CommonStore &common;
  ProductionAssetLifecycleState &state;
  // 真实 WorldBundle 私有候选的普通字段 owner；不是旧 clone 额度或临时壳。
  CheckpointMemoryCharge &owned_fields;
  const ProductionAssetLifecycleReadContext &read;
};
// 仅本次候选内的行索引，既不是持久来源也不是授权。调用方在原 command
// seal 后立即绑定，任何失败必须弃置整个候选，不可在已移动前像上重试。
struct ProductionAssetPendingCommand {
  enum class Kind { seller_offer, buyer_transfer } kind = Kind::seller_offer;
  std::size_t row_index = 0;
  std::optional<std::size_t> acceptance_index;
  std::optional<std::size_t> resource_execution_index;
};
// 原 AcceptPropertyTransfer 的卖方采用阶段；实际 actor 由 S0 envelope 给出。
// 只写真实卖方同意 offer，不扣买方钱，不提前转产权或借用买方同意。
Outcome<ProductionAssetPendingCommand>
accept_production_asset_transfer_quote_in_candidate(
    ProductionAssetLifecycleWriteContext &, const StableId &seller_actor,
    const StableId &asset, const StableId &buyer_actor, Integer price,
    const MomentOwnerSource &expected_title);
// 原指定买方本人接受唯一原 offer。在同私有候选执行 Common 现金双腿并
// 写实际交割；此阶段绝不能发布。后续必须完成原工程/使用权/holder 处置，
// 再由实际 S0 成功结果封口，不能以产权行已写替代这些必要后继。
Outcome<ProductionAssetPendingCommand>
accept_production_asset_transfer_offer_in_candidate(
    ProductionAssetLifecycleWriteContext &, const StableId &buyer_actor,
    const SourceRef &accepted_offer_ref);
// 借未封口候选的实际行准备原 LE 回执；不分配新事实或替代成功 command。
Outcome<const ProductionAssetTransferOffer *>
read_pending_production_asset_offer(
    const ProductionAssetLifecycleWriteContext &,
    const ProductionAssetPendingCommand &);
// 原成功 Common 回执已由 S0 seal 缓存后晚绑，同形临时回执或错本人拒。
// 绑定后立即走本域完整产权源反读；失败仍必须弃置外层整个候选。
Status bind_production_asset_offer_command_result_in_candidate(
    ProductionAssetLifecycleWriteContext &,
    const ProductionAssetPendingCommand &, const CommitResult &);
} // namespace hanmo::resident
