// E §15.3 / S6 B1 本人真实资源报价采用与同意的两种 native 输入。
// 尚未登记 Command variant；不因此启用 foundation_assets/1 或完整成立。
#pragma once
#include "resident/kernel/world_timeline.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
inline constexpr const char *offer_foundation_assets_command =
    "offer_foundation_assets";
inline constexpr const char *consent_foundation_assets_command =
    "consent_foundation_assets";
// Owner 只能取原 envelope 的真实本人 actor；不接未来组织、caller授权布尔、
// 自由余额或预付款。明确钱/批/窗口先产生原offer，未来intent不得在这里预写。
struct OfferFoundationAssets {
  StableId founder_actor_id, wallet_id;
  Integer cash_quantity = 0;
  StableId inventory_id;
  std::vector<CargoSelection> selected_lots;
  MomentSpan commitment_window;
};
// 只同意原已存在且冻结此offer的同版意向，原本人必须实际获知其内容。
// 同意不执行预约/付款；后继全部成立只能经原同候选资源与S6完整门。
struct ConsentFoundationAssets {
  SourceRef original_offer;
  MomentOwnerSource actual_intent;
  Integer expected_terms_revision = 1;
  MomentOwnerSource actual_content_receipt;
};
} // namespace hanmo::resident
