// S7唯一家庭买方成交；S5只存本原收据，实际货仍由Common精确选批转移。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "private_shop.hpp"
#include "resident/domains/growth/growth_market_read.hpp"
#include "resident/domains/local_economy/household_purchase_types.hpp"
#include "verified_trade_recipe.hpp"
namespace hanmo::resident {
struct HouseholdRetailCatalog {
  Integer version = 1;
  std::vector<StableId> premises_ids, public_market_shop_ids;
};
struct HouseholdRetailTerms {
  StableId purchase_id, household_group_id;
  HouseholdBuyer executor;
  HouseholdRetailSeller seller_ref;
  StableId item_id, buyer_wallet_id, seller_wallet_id, source_inventory_id,
      custody_inventory_id, destination_inventory_id, place_id;
  Owner buyer, seller;
  Integer quantity = 0, total = 0, market_revision = 0, market_stock = 0,
          base_price = 0, midpoint = 0, unit_price = 0;
  WorldMoment quoted_at;
  std::vector<CargoSelection> selected_cargo;
  SourceRef accepted_terms_ref, custody_terms_ref;
  Integer terms_version = 0;
};
struct HouseholdRetailSettlement {
  StableId id;
  HouseholdRetailTerms terms;
  WorldMoment settled_at;
  Integer market_version_after = 0, cost_of_goods = 0;
  std::vector<VerifiedTradeLot> lots;
  std::vector<PrivateShopRetailLot> consumed_lots;
  // 冻结双腿只作原收据校验，不是可花钱包、成交余额或当前货物位置。
  Integer payer_balance_before = 0, payer_balance_after = 0,
          payee_balance_before = 0, payee_balance_after = 0;
  SourceRef payment_ref;
  std::vector<SourceRef> transfer_refs;
};
struct HouseholdRetailState {
  Integer version = 1;
  OrderedTable<HouseholdRetailSettlement> settlements;
};
struct HouseholdRetailQuote {
  WorldIdentity identity;
  HouseholdRetailTerms terms;
  std::vector<ReadVersion> versions;
};
struct HouseholdRetailAcceptance {
  StableId purchase_id, household_group_id, wallet_id, custody_inventory_id,
      destination_inventory_id;
  Owner owner;
  HouseholdBuyer executor;
  SourceRef accepted_terms_ref, custody_terms_ref;
  Integer merchandise_limit = 0, terms_version = 0;
  WorldMoment accepted_at;
};
class HouseholdRetailAcceptanceRead {
public:
  virtual ~HouseholdRetailAcceptanceRead() = default;
  virtual Outcome<HouseholdRetailAcceptance>
  read_household_retail_acceptance(const StableId &purchase_id) const = 0;
  virtual Status validate_household_retail_settlement(
      const HouseholdRetailSettlement &) const = 0;
};
// 公有市场的保护写scope沿原MarketBusinessCapture扩展；这里不绕Common保护门。
class HouseholdRetailMarketCapture {
public:
  virtual ~HouseholdRetailMarketCapture() = default;
  virtual Outcome<VerifiedTradeExecution>
  apply_household_retail_recipe_in_candidate(CommonStore &,
                                             const VerifiedTradeTerms &,
                                             const HouseholdRetailTerms &) = 0;
  virtual Status
  bind_household_retail_source(const HouseholdRetailSettlement &) = 0;
};
struct HouseholdRetailReadContext {
  const CommonStore &common;
  const CommonCatalog &common_catalog;
  const PrivateShopState &shops;
  const PrivateShopCatalog *shops_catalog;
  const GrowthCatalog &markets;
  const HouseholdRetailCatalog &catalog;
  const HouseholdRetailState &state;
  const WorldMoment &now;
  const HouseholdRetailAcceptanceRead &acceptances;
  const PrivateShopMarketHistoryRead *market_history = nullptr;
  HouseholdRetailMarketCapture *market_capture = nullptr;
  HouseholdRetailReadContext(
      const CommonStore &c, const CommonCatalog &cc, const PrivateShopState &s,
      const PrivateShopCatalog *sc, const GrowthCatalog &m,
      const HouseholdRetailCatalog &k, const HouseholdRetailState &h,
      const WorldMoment &n, const HouseholdRetailAcceptanceRead &a,
      const PrivateShopMarketHistoryRead *history = nullptr,
      HouseholdRetailMarketCapture *capture = nullptr)
      : common(c), common_catalog(cc), shops(s), shops_catalog(sc), markets(m),
        catalog(k), state(h), now(n), acceptances(a), market_history(history),
        market_capture(capture) {}
  HouseholdRetailReadContext(
      const CommonStore &c, const CommonCatalog &cc, const PrivateShopState &s,
      const PrivateShopCatalog &sc, const GrowthCatalog &m,
      const HouseholdRetailCatalog &k, const HouseholdRetailState &h,
      const WorldMoment &n, const HouseholdRetailAcceptanceRead &a,
      const PrivateShopMarketHistoryRead *history = nullptr,
      HouseholdRetailMarketCapture *capture = nullptr)
      : HouseholdRetailReadContext(c, cc, s, &sc, m, k, h, n, a, history,
                                   capture) {}
};
bool household_retail_selected(const PrivateShopContext &,
                               const StableId &shop_id);
bool household_retail_operation_selected(const PrivateShopContext &,
                                         const StableId &operation_id);
Outcome<HouseholdRetailQuote>
preview_household_retail(const HouseholdRetailReadContext &,
                         const HouseholdRetailTerms &request);
Outcome<StableId>
settle_household_retail_in_candidate(CommonStore &, HouseholdRetailState &,
                                     const HouseholdRetailReadContext &,
                                     const HouseholdRetailQuote &);
Outcome<const HouseholdRetailSettlement *> read_household_retail_settlement(
    const CommonStore &, const HouseholdRetailState &, const StableId &);
Status validate_household_retail(const HouseholdRetailReadContext &);
Status summarize_household_retail_operation(const PrivateShopContext &,
                                            const StableId &,
                                            PrivateShopDayResult &);
Status validate_household_retail_operation_result(const PrivateShopContext &,
                                                  const PrivateShopOperation &,
                                                  const PrivateShopDayResult &);
const std::vector<PrivateShopRetailLot> *
private_shop_actual_cost_lots(const PrivateShopContext &,
                              const StableId &fact_id);
Outcome<HouseholdRetailCatalog>
decode_household_retail_catalog(const BoundaryValue &);
Outcome<HouseholdRetailState>
decode_household_retail_state(const BoundaryValue &);
BoundaryValue encode_household_retail_state(const HouseholdRetailState &);
Status visit_household_retail_checkpoint(CodecVisitor &,
                                         HouseholdRetailState &);
class ProjectionWriter;
BoundaryValue encode_household_retail_catalog(const HouseholdRetailCatalog &);
Status visit_household_retail_catalog(CodecVisitor &, HouseholdRetailCatalog &);
Status emit_household_retail_state(ProjectionWriter &,
                                   const HouseholdRetailState &);
Status emit_household_retail_catalog(ProjectionWriter &,
                                     const HouseholdRetailCatalog &);

} // namespace hanmo::resident
