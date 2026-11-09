#include "economic_market_capture_inputs.hpp"

namespace hanmo::resident {
Outcome<EconomicMarketCaptureInputsBorrow>
read_economic_market_capture_inputs(const CommonStore &common,
                                    const LocalEconomyState &state,
                                    const LocalEconomyCatalog &catalog,
                                    MarketCauseKind kind, const StableId &key) {
  auto fail =
      [&](const char *code) -> Outcome<EconomicMarketCaptureInputsBorrow> {
    return {false, {}, common_failure(code, key).error};
  };
  if (!catalog.enabled || key.empty())
    return fail("EconomicMarketCaptureOriginalBusinessRequired");
  if (kind == MarketCauseKind::local_production) {
    const auto *original = catalog.production_units.find(key);
    const auto *actual = state.production_units.find(key);
    if (!original || !actual || original->id != key || actual->id != key ||
        actual->recipe_id != original->recipe_id ||
        actual->input_inventory_ids != original->input_inventory_ids ||
        actual->output_inventory_id != original->output_inventory_id ||
        !owner_equal(actual->owner, original->owner))
      return fail("EconomicMarketCaptureOriginalProductionMismatch");
    const auto *recipe = catalog.recipes.find(original->recipe_id);
    if (!recipe || recipe->id != original->recipe_id)
      return fail("EconomicMarketCaptureOriginalRecipeRequired");
    for (const auto &id : original->input_inventory_ids)
      if (!common.inventory(id))
        return fail("EconomicMarketCaptureInputInventoryRequired");
    if (!common.inventory(original->output_inventory_id))
      return fail("EconomicMarketCaptureOutputInventoryRequired");
    return {true,
            {original->input_inventory_ids, &original->output_inventory_id,
             nullptr},
            {}};
  }
  if (kind != MarketCauseKind::local_ration)
    return fail("EconomicMarketCaptureOriginalKindMismatch");
  const auto *original = catalog.obligations.find(key);
  const auto *actual = state.obligations.find(key);
  if (!original || !actual || original->id != key || actual->id != key ||
      original->kind != ObligationKind::ration ||
      actual->kind != ObligationKind::ration ||
      actual->source_wallet_or_inventory !=
          original->source_wallet_or_inventory ||
      actual->item_id != original->item_id ||
      actual->rate_profile != original->rate_profile ||
      actual->principal.kind != original->principal.kind ||
      actual->principal.id != original->principal.id ||
      actual->beneficiary.kind != original->beneficiary.kind ||
      actual->beneficiary.id != original->beneficiary.id)
    return fail("EconomicMarketCaptureOriginalRationMismatch");
  const auto *rate = catalog.rates.find(original->rate_profile);
  if (!rate || rate->id != original->rate_profile ||
      !common.inventory(original->source_wallet_or_inventory))
    return fail("EconomicMarketCaptureRationInventoryRequired");
  return {true, {{}, nullptr, &original->source_wallet_or_inventory}, {}};
}
} // namespace hanmo::resident
