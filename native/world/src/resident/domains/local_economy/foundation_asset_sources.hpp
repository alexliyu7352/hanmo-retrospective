// E §15.3 / S6 B1 的首次私人实际投入来源草案。原余额/库存/预约仍只在
// Common；这些是不可变 offer、本人同意和已发生的执行结果，不是第二账。
#pragma once
#include "foundation_asset_resource_effects.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/model/common_model.hpp"

namespace hanmo::resident {
inline constexpr const char *foundation_assets_module = "foundation_assets";
inline constexpr const char *foundation_asset_offer_kind =
    "foundation_asset_offered";
inline constexpr const char *foundation_asset_consent_kind =
    "foundation_asset_consented";
inline constexpr const char *foundation_asset_prepared_kind =
    "foundation_asset_prepared";
inline constexpr const char *foundation_asset_fulfilled_kind =
    "foundation_asset_fulfilled";

// 精确原批冻结选择数量和原物身份。后生拆批必须保存原 Common 实际映射，
// 不从今日库存数量或同类物品重新选择一批以冒原同意。
struct FoundationOfferedLot {
  CargoSelection selection;
  StableId item_id;
  std::optional<StableId> item_instance_id;
  std::optional<LotPropertiesRef> properties_ref;
};
// 只有原私人捐助分支；recipient 先绑定真实 founder，后生同意另锁整个
// actual intent。尚未存在的组织绝不出现在 Owner 或 Common 资源容器中。
struct FoundationAssetOfferTerms {
  StableId owner_actor_id, founder_actor_id;
  StableId wallet_id;
  Integer cash_quantity = 0;
  StableId inventory_id;
  std::vector<FoundationOfferedLot> lots;
  MomentSpan commitment_window;
};
struct FoundationAssetOffer {
  StableId id;
  FoundationAssetOfferTerms terms;
  WorldMoment offered_at;
  SourceRef owner_command_source;
};
// 同意指定已实际发生的意向/版本，而非只同意加入。actual_content_receipt
// 指原同版披露，不能将本人获知或 FoundationSupport 单独当捐助授权。
struct FoundationAssetConsent {
  StableId id, offer_id;
  MomentOwnerSource intent_source;
  Integer intent_terms_revision = 1;
  MomentOwnerSource actual_content_receipt;
  WorldMoment consented_at;
  SourceRef owner_command_source;
};
// quantity/item/Owner 来自 offer 的同序原批。这里只记实际 ReserveSelectedLots
// 的源 lot→已预约 lot 映射，预约金额仍从 Common 反读，不保存剩余余额。
struct FoundationAssetPreparedLot {
  StableId original_lot_id, reserved_lot_id;
};
struct FoundationAssetPreparation {
  StableId id, consent_id;
  WorldMoment prepared_at;
  std::vector<FoundationAssetPreparedLot> actual_lots;
  FoundationAssetPreparationEffects actual_resources;
};
// 受领组织必须已在同候选实际存在，随后原登记最终门才可封口。这里只
// 记录真实钱/货原语的去向与映射，不将 reservation 改名贡献或预写未来 ID。
struct FoundationAssetDeliveredLot {
  StableId reserved_lot_id, delivered_lot_id;
};
struct FoundationAssetFulfillment {
  StableId id, preparation_id, organization_id;
  StableId destination_wallet_id, destination_inventory_id;
  WorldMoment fulfilled_at;
  std::vector<FoundationAssetDeliveredLot> actual_lots;
  FoundationAssetFulfillmentEffects actual_resources;
};
// 当前有限分支的预约只存在于准备→决定→安装→履行→登记的同一未发布候选；任何
// 晚失败整候选弃置，既往已发布同意不变。稳定出口每份预约必须恰一履行。
// 不支持独立长期预约，所以不造取消/到期事实或另一份释放账。
// 首次 /1 尚未选入 World。原字段表、clone/copy/retain、真实命令与反向
// Common 覆盖全部齐备前不可发布；容器只为同 job 真实内存所有权。
struct FoundationAssetSourcesState {
  Integer schema_version = 1;
  CheckpointVector<FoundationAssetOffer> offers;
  CheckpointVector<FoundationAssetConsent> consents;
  CheckpointVector<FoundationAssetPreparation> preparations;
  CheckpointVector<FoundationAssetFulfillment> fulfillments;
};
} // namespace hanmo::resident
