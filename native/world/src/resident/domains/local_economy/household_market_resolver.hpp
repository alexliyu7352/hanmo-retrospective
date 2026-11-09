#pragma once
#include "private_shop_household_custody.hpp"
namespace hanmo::resident {
// 按明确报价卖方解析公共服务或实际私人托管，不猜选卖方。
Outcome<const HouseholdMarketServiceDefinition *>
read_household_market_service_preview(
    const CommonStore &, const HouseholdPurchaseCatalog &,
    const HouseholdPurchaseState &, const PrivateShopHouseholdCustodyContext *,
    const HouseholdPurchasePlan &, const HouseholdRetailSeller &);
// 按拥有根内实际采购接受反读原服务；异根接受不得借用。
Outcome<const HouseholdMarketServiceDefinition *>
read_household_market_service_accepted(
    const CommonStore &, const HouseholdPurchaseCatalog &,
    const HouseholdPurchaseState &, const PrivateShopHouseholdCustodyContext *,
    const HouseholdPurchaseAcceptedTerms &);
// 仅有计划的调用者只能借唯一实际接受或原公共目录，不暗选私人卖家。
Outcome<const HouseholdMarketServiceDefinition *>
read_household_market_service_for_plan(
    const CommonStore &, const HouseholdPurchaseCatalog &,
    const HouseholdPurchaseState &, const PrivateShopHouseholdCustodyContext *,
    const HouseholdPurchasePlan &);
// 核服务指针确属原目录或原接受，拒 caller 复制伪造条件。
Status validate_household_resolved_market_service(
    const CommonStore &, const HouseholdPurchaseCatalog &,
    const HouseholdPurchaseState &, const PrivateShopHouseholdCustodyContext *,
    const HouseholdPurchasePlan &,
    const HouseholdMarketServiceDefinition &actual_service);
// 仅返回同市场无歧义原地点，不能作为报价知情或托管同意。
Outcome<const StableId *> read_household_original_market_place(
    const CommonStore &, const HouseholdPurchaseCatalog &,
    const HouseholdPurchaseState &, const PrivateShopHouseholdCustodyContext *,
    const StableId &market_ref);
// 仅证明作者目录中存在该市场，不证明任何角色或 cohort 已知报价。
Outcome<bool> household_market_catalog_contains(
    const CommonStore &, const HouseholdPurchaseCatalog &,
    const HouseholdPurchaseState &, const PrivateShopHouseholdCustodyContext *,
    const StableId &);
} // namespace hanmo::resident
