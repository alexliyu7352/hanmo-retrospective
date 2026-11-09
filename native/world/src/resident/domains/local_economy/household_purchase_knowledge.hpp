#pragma once
#include "household_purchase_demand.hpp"
#include "resident/domains/social/known_supply_read2.hpp"
namespace hanmo::resident {
struct HouseholdPurchaseReportContext;
class NativeHouseholdPurchaseKnownMarketRead final
    : public HouseholdPurchaseKnownMarketRead {
  const CommonStore &common_;
  const CommonCatalog &catalog_;
  const SocialState &social_;
  const ExactResultTimeRead &sources_;
  const ExactResultReadStamp &stamp_;
  const HouseholdPurchaseReportContext *plan_reports_ = nullptr;
  HouseholdMarketKnowledgeSelection selection_;

public:
  NativeHouseholdPurchaseKnownMarketRead(
      const CommonStore &common, const CommonCatalog &catalog,
      const SocialState &social, const ExactResultTimeRead &sources,
      const ExactResultReadStamp &stamp,
      const HouseholdPurchaseReportContext *plan_reports = nullptr,
      HouseholdMarketKnowledgeSelection selection =
          HouseholdMarketKnowledgeSelection::original_subject)
      : common_(common), catalog_(catalog), social_(social), sources_(sources),
        stamp_(stamp), plan_reports_(plan_reports), selection_(selection) {}
  Outcome<HouseholdKnownMarketEvidence> read_known_household_market(
      const HouseholdPurchasePlan &, const StableId &, const StableId &,
      const StableId &, const StableId &, const WorldMoment &) const override;
};
} // namespace hanmo::resident
