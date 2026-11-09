// 本人私产交割只读责任口：原租约未真实关闭时不得把个人资产清空来冒商铺免责。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
class OriginalWorldReadBoundary;
struct PrivateShopPrivateAssetScope {
  StableId actor_id;
  std::vector<StableId> wallet_ids, inventory_ids;
};
struct PrivateShopPrivateAssetBlocker {
  StableId kind, source_id;
};
struct PrivateShopPrivateAssetResponsibilities {
  WorldIdentity source_identity;
  PrivateShopPrivateAssetScope scope;
  Integer tick = 0;
  StableId before_fact_id;
  std::vector<PrivateShopPrivateAssetBlocker> blockers;
};
// 同World须已完成Shop/LE/Task全验；本口不反调资格/政治/继产，调用方另证原容器产权。
Outcome<PrivateShopPrivateAssetResponsibilities>
read_private_shop_private_asset_responsibilities(
    const CommonStore &, const PrivateShopState *,
    const PrivateShopPrivateAssetScope &);
// 历史只认原真实family_inheritance_settled；payload
// actor/tick/选择由S4先验，不拿当前关闭状态倒填。
Outcome<PrivateShopPrivateAssetResponsibilities>
read_private_shop_private_asset_responsibilities_before_inheritance(
    const CommonStore &, const PrivateShopState *,
    const PrivateShopPrivateAssetScope &, const StableId &actual_fact_id,
    Integer tick);
// 政治早位仅核原租金/工资的本账结构和精确原头；不回调法律、Task、Growth或完整Shop。
// 最终World仍必须完整Shop及政治复核，本口不证明经营资格或付款历史。
struct PrivateShopResponsibilitySourceContext {
  const CommonStore &common;
  const LocalEconomyState &economy;
  const LocalEconomyCatalog &economy_catalog;
  const PrivateShopState &state;
  const PrivateShopCatalog &catalog;
  const DynamicStepInput *phase = nullptr;
  const PrivateShopDailyRentCatalog *daily_rent_catalog = nullptr;
  const PrivateShopDailyRentState *daily_rent = nullptr;
  const OriginalWorldReadBoundary *read_boundary3 = nullptr;
  const CurrentMomentRead *moment_clock3 = nullptr;
};
Status validate_private_shop_public_responsibility_sources(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    const PrivateShopState &, const PrivateShopCatalog &,
    const DynamicStepInput *phase = nullptr,
    const OriginalWorldReadBoundary *read_boundary3 = nullptr,
    const CurrentMomentRead *moment_clock3 = nullptr);
// 只给原责任结构/付款原读共享上界；不产生时刻事实、不替代原资金验证。
Outcome<Integer> read_private_shop_responsibility_observed_tick(
    const PrivateShopResponsibilitySourceContext &);
// LE
// 早位逐原租/工资业务证明私约及其唯一钱账；终止半日余数只豁免精确原补尾应付。
struct PrivateShopEconomySourceIds {
  std::vector<StableId> obligation_ids, fractional_arrear_ids;
};
Outcome<PrivateShopEconomySourceIds> read_private_shop_economy_source_ids(
    const PrivateShopResponsibilitySourceContext &);
// 上述具名结构先验后，只证明真实私租/私工资与本次公共选择无交集，不授公产继受。
struct EconomicPublicAccountSelection;
Outcome<bool> private_shop_obligation_outside_public_selection(
    const PrivateShopResponsibilitySourceContext &, const RecurringObligation &,
    const EconomicPublicAccountSelection &);
Outcome<bool> private_shop_obligation_outside_public_selection(
    const PrivateShopContext &, const RecurringObligation &,
    const EconomicPublicAccountSelection &);
} // namespace hanmo::resident
