// 初货的真正分配仍由中央原native seeder执行；这里只验证并冻结其typed结果。
#include "growth_market_initial.hpp"
#include "growth_model.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
Outcome<GrowthMarketHistoryCatalog> make_initial_catalog(
    const CommonCatalog &common, const GrowthCatalog &growth,
    const OrderedTable<InventoryView> &assets, Integer initial_next_instance_id,
    const std::vector<StableId> &shops, Integer selected_version) {
  auto bad = [](const StableId &code,
                const StableId &path =
                    StableId{}) -> Outcome<GrowthMarketHistoryCatalog> {
    return {false, {}, common_failure(code, path).error};
  };
  if (!growth.economy || !growth.regional_life || shops.empty() ||
      shops.size() > 4096)
    return bad("GrowthMarketInitialProfileInvalid");
  if (initial_next_instance_id < 1)
    return bad("GrowthMarketOriginalNextInstanceMismatch");
  // 原seeder先按有效inventories顺序分初货，再按原actors顺序分bag初货。
  // 无其他实例分配的这一截面必须恰好连续；拒今天数据伪装成少了旧批次的初态。
  Integer next = 1;
  for (const auto &row : assets.rows()) {
    const auto &inv = row.second;
    if (inv.id != row.first || inv.revision != 0)
      return bad("GrowthMarketOriginalInstalledAssetsRequired", row.first);
    for (const auto &lot : inv.lots) {
      if (next == INT64_MAX || lot.id != "lot:" + std::to_string(next) ||
          lot.quantity <= 0 || !lot.reserved_by.empty() ||
          !owner_equal(lot.owner, inv.owner) || !common.items.find(lot.item_id))
        return bad("GrowthMarketOriginalInstalledLotInvalid", lot.id);
      ++next;
    }
  }
  if (next != initial_next_instance_id)
    return bad("GrowthMarketOriginalNextInstanceMismatch");
  GrowthMarketHistoryCatalog result;
  result.selected_version = selected_version;
  result.shop_ids = shops;
  result.initial_next_instance_id = next;
  std::set<StableId> unique;
  for (const auto &id : shops) {
    const auto *shop = growth.shops.find(id);
    if (!unique.insert(id).second || !shop)
      return bad("GrowthMarketInitialSelectionInvalid", id);
    const auto *required = common.required_inventories.find(shop->inventory_id);
    const auto *inventory = assets.find(shop->inventory_id);
    if (!required || !inventory ||
        !owner_equal(required->owner, inventory->owner) ||
        required->capacity != inventory->capacity || inventory->capacity < 0)
      return bad("GrowthMarketInitialSourceInvalid", id);
    for (const auto &actor : common.actor_ids)
      if (shop->inventory_id == "bag:" + actor)
        return bad("GrowthMarketNotDedicated", id);
    Integer weight = 0;
    for (const auto &lot : inventory->lots) {
      const auto item = common.items.find(lot.item_id);
      if (item->weight < 0 ||
          (item->weight && lot.quantity > (INT64_MAX - weight) / item->weight))
        return bad("GrowthMarketInitialWeightOverflow", id);
      weight += lot.quantity * item->weight;
    }
    if (weight > inventory->capacity ||
        !result.initial_inventories.insert(inventory->id, *inventory))
      return bad("GrowthMarketInitialSelectionInvalid", id);
  }
  return {true, std::move(result), {}};
}
Outcome<std::vector<StableId>> decode_selection(const BoundaryValue &value,
                                                Integer selected_version) {
  try {
    using namespace boundary;
    fields(value, {"version", "shop_ids"}, {}, "growth_market_history");
    if (object(value, "").size() != 2 ||
        get_int(value, "version", "") != selected_version)
      fail("UnsupportedGrowthMarketHistoryVersion", "growth_market_history");
    auto shops = strings(field(value, "shop_ids", ""), "shop_ids");
    if (shops.empty() || shops.size() > 4096 ||
        std::set<StableId>(shops.begin(), shops.end()).size() != shops.size())
      fail("GrowthMarketInitialSelectionInvalid", "shop_ids");
    for (const auto &id : shops)
      if (id.empty() || id.size() > 256)
        fail("GrowthMarketInitialSelectionInvalid", id);
    return {true, std::move(shops), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace
Outcome<GrowthMarketHistoryCatalog> make_growth_market_initial_catalog(
    const CommonCatalog &common, const GrowthCatalog &growth,
    const OrderedTable<InventoryView> &assets, Integer next,
    const std::vector<StableId> &shops) {
  return make_initial_catalog(common, growth, assets, next, shops, 1);
}
Outcome<GrowthMarketHistoryCatalog> make_growth_market_initial_catalog_v2(
    const CommonCatalog &common, const GrowthCatalog &growth,
    const OrderedTable<InventoryView> &assets, Integer next,
    const std::vector<StableId> &shops) {
  return make_initial_catalog(common, growth, assets, next, shops, 2);
}
Outcome<std::vector<StableId>>
decode_growth_market_history_selection(const BoundaryValue &value) {
  return decode_selection(value, 1);
}
Outcome<std::vector<StableId>>
decode_growth_market_history_selection_v2(const BoundaryValue &value) {
  return decode_selection(value, 2);
}
} // namespace hanmo::resident
