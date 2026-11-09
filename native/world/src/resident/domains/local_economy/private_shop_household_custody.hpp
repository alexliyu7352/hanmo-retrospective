// 显式营业托管同意；独立原源，不改变营业 /2 的既有字节。
#pragma once
#include "household_private_market_basis.hpp"
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
// 只允许原政策显式选择店主私人库；不允许作者预填未来营业库标识。
enum class PrivateShopCustodyInventorySelection {
  proprietor_personal_inventory
};
// 锁定原铺位、有限容器选择与真实装卸规则；不另设费用。
struct PrivateShopHouseholdCustodyPolicy {
  ActorFoundationRuleRef policy_ref;
  StableId premises_id;
  PrivateShopCustodyInventorySelection inventory_selection =
      PrivateShopCustodyInventorySelection::proprietor_personal_inventory;
  StableId handling_rule_id;
  Integer handling_rule_version = 1;
};
// 同内容身份的唯一托管政策目录；未知格式不得默认启用。
struct PrivateShopHouseholdCustodyCatalog {
  Integer version = 1;
  StableId catalog_digest;
  OrderedTable<PrivateShopHouseholdCustodyPolicy> policies;
};
// 保存实际营业当次明确同意及由原源解析的服务条件。
struct PrivateShopHouseholdCustodyAcceptedSource {
  StableId id, operation_id, proprietor_actor_id;
  SourceRef original_shop_epoch;
  ActorFoundationRuleRef explicit_policy_ref;
  WorldMoment accepted_at;
  // 不可变实际接受原件持有解析后的真实标识；作者不预造未来标识。
  HouseholdMarketServiceDefinition accepted_service;
};
// 唯一拥有托管接受原件；不拥有另一份库存或产权状态。
struct PrivateShopHouseholdCustodyState {
  Integer version = 1;
  OrderedTable<PrivateShopHouseholdCustodyAcceptedSource> accepted;
};
// 借用同世界营业、私产历史和装卸目录，生命周期由外层 lease 持有。
struct PrivateShopHouseholdCustodyContext {
  const PrivateHouseholdMarketBasisContext &market;
  const TravelCatalog &travel;
  const PrivateShopHouseholdCustodyCatalog &catalog;
  const PrivateShopHouseholdCustodyState &state;
};
// 构造此有限原源类别的引用；真实性仍须实际根和 Common 头验证。
SourceRef private_shop_household_custody_source(const StableId &);
// 核同图、内容身份、原铺位和装卸规则，空接受根也必须过此门。
Status validate_private_shop_household_custody_catalog(
    const PrivateShopHouseholdCustodyContext &);
// 回读实际营业、明确政策和接受前产权；缺头、错序或异图即拒。
Outcome<const PrivateShopHouseholdCustodyAcceptedSource *>
read_private_shop_household_custody(const PrivateShopHouseholdCustodyContext &,
                                    const MomentOwnerSource &);
// 逐原件验证并反扫孤立 Common 头，不以表空替代选型。
Status validate_private_shop_household_custody_sources(
    const PrivateShopHouseholdCustodyContext &);
// 纯预览当前店主、原铺位和明确政策；不分配标识，不产生同意。
Status preview_private_shop_household_custody_choice(
    const PrivateShopHouseholdCustodyContext &, const StableId &actual_shop_id,
    const ActorFoundationRuleRef &explicit_policy,
    const StableId &actual_actor_id);
// 操作者和明确政策来自真实命令；不存在默认同意。
// 同候选在实际营业接受后写真托管原件；失败必须撤回整个候选。
Outcome<StableId> accept_private_shop_household_custody_in_candidate(
    CommonStore &, PrivateShopHouseholdCustodyState &,
    const PrivateShopHouseholdCustodyContext &,
    const StableId &actual_operation_id,
    const ActorFoundationRuleRef &explicit_policy,
    const StableId &actual_actor_id);
// 当前卖方须实际自行营业且已有明确同意，返回其唯一服务原件。
Outcome<const HouseholdMarketServiceDefinition *>
read_private_shop_household_service_current(
    const PrivateShopHouseholdCustodyContext &,
    const PrivateHouseholdRetailSeller &);
// 采购冷读只借该原接受之前的同营业托管，不从今日状态倒填。
Outcome<const HouseholdMarketServiceDefinition *>
read_private_shop_household_service_accepted(
    const PrivateShopHouseholdCustodyContext &,
    const HouseholdPurchaseAcceptedTerms &);
} // namespace hanmo::resident
