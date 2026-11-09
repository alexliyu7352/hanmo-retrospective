// S5原接受来源的独立只读适配，避免S5 Context与S7 Acceptance循环构造。
#pragma once
#include "household_purchase_contract.hpp"
namespace hanmo::resident {
struct HouseholdRetailAcceptanceContext {
  const CommonStore &common;
  const LocalEconomyState &economy;
  const HouseholdPurchaseCatalog &catalog;
  const HouseholdPurchaseState &state;
  const HouseholdProvisionControlRead &controls;
  const PopulationTransportState *population = nullptr;
  const TransportRegistrationState *transport_registrations = nullptr;
  const PrivateShopHouseholdCustodyContext *private_market_custody = nullptr;
};
class NativeHouseholdRetailAcceptanceRead final
    : public HouseholdRetailAcceptanceRead {
  HouseholdRetailAcceptanceContext x_;

public:
  explicit NativeHouseholdRetailAcceptanceRead(
      HouseholdRetailAcceptanceContext x)
      : x_(x) {}
  Outcome<HouseholdRetailAcceptance>
  read_household_retail_acceptance(const StableId &) const override;
  Status validate_household_retail_settlement(
      const HouseholdRetailSettlement &) const override;
};
} // namespace hanmo::resident
