// 固定05的有限储粮规则：纯投影真实私粮、已经成交的在途货和原受领预算。
#pragma once
#include "household_purchase_contract.hpp"
#include "resident/domains/interaction/household_purchase_report_model.hpp"
#include "resident/domains/social/social_time2_model.hpp"
namespace hanmo::resident {
struct HouseholdReserveDemand {
  SourceRef reserve_rule_ref;
  StableId plan_id, household_id, item_id, destination_inventory_id;
  Integer available_stock = 0, owned_pending_delivery = 0, minimum_stock = 0,
          quantity_needed = 0;
  Integer minimum_cash_reserve = 0, available_cash_after_reserve = 0;
  std::vector<SourceRef> pending_retail_sources;
  std::vector<ReadVersion> versions;
};
enum class HouseholdMarketKnowledgeSelection {
  original_subject = 1,
  accepted_plan_scope = 2
};
struct HouseholdKnownMarketEvidence {
  // Own the selected knowledge receipt and copied source IDs for the lifetime
  // of the proposal, rather than borrowing the temporary Social query charge.
  CheckpointMemoryCharge memory;
  HouseholdKnownMarketEvidence() = default;
  HouseholdKnownMarketEvidence(const HouseholdKnownMarketEvidence &) = delete;
  HouseholdKnownMarketEvidence &
  operator=(const HouseholdKnownMarketEvidence &) = delete;
  HouseholdKnownMarketEvidence(HouseholdKnownMarketEvidence &&) = default;
  HouseholdKnownMarketEvidence &
  operator=(HouseholdKnownMarketEvidence &&) = delete;
  HouseholdMarketKnowledgeSelection selection =
      HouseholdMarketKnowledgeSelection::original_subject;
  SourceRef report_ref, observation_source;
  std::variant<KnowledgeReceiptV2, HouseholdPurchaseScopedKnowledgeRef>
      knowledge;
  WorldMoment observed_at;
};
const StableId &
household_market_known_report_id(const HouseholdKnownMarketEvidence &);
bool household_market_knowledge_belongs_to_plan(
    const HouseholdKnownMarketEvidence &, const HouseholdPurchasePlan &);
// S3 显式选型借原本人 receipt 或本计划实际告知；市场与现价仍由原 S7 核验。
class HouseholdPurchaseKnownMarketRead {
public:
  virtual ~HouseholdPurchaseKnownMarketRead() = default;
  virtual Outcome<HouseholdKnownMarketEvidence> read_known_household_market(
      const HouseholdPurchasePlan &, const StableId &market_ref,
      const StableId &inventory_id, const StableId &place_id,
      const StableId &item_id, const WorldMoment &) const = 0;
};
struct HouseholdReservePurchaseProposal {
  HouseholdReserveDemand demand;
  HouseholdKnownMarketEvidence known_market;
  HouseholdPurchaseQuote quote;
};
Status validate_household_reserve_rules(const CommonStore &,
                                        const CommonCatalog &,
                                        const LocalEconomyState &,
                                        const HouseholdPurchaseCatalog &);
Outcome<std::vector<HouseholdReserveDemand>>
read_household_reserve_demand(const HouseholdPurchaseContext &,
                              const StableId &plan_id);
// 仅从实际需求选一笔确定报价；缺钱/无缺口返回合法none，技术/权限/来源错误照常失败。
Outcome<std::optional<HouseholdReservePurchaseProposal>>
preview_household_reserve_purchase(const HouseholdPurchaseContext &,
                                   const HouseholdPurchaseKnownMarketRead &,
                                   const StableId &plan_id,
                                   const HouseholdRetailSeller &);
// 采用前重新读真实缺口、知情、钱/货与整个报价；不按新价或新数量偷偷采用旧proposal。
Outcome<StableId> accept_household_reserve_purchase_in_candidate(
    CommonStore &, LocalEconomyState &, HouseholdPurchaseState &,
    FacilityClaimsState &, ActionElapsedStore &, HouseholdRetailState &,
    const HouseholdPurchaseContext &, const HouseholdPurchaseKnownMarketRead &,
    const HouseholdReservePurchaseProposal &);
} // namespace hanmo::resident
