// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.household_purchases) {
  if (!catalog.household_retail_settlements ||
      (catalog.selected_household_version != 2 &&
       catalog.selected_household_version != 3))
    fail("HouseholdCatalogSelectionMismatch", "household_purchases");
  {
    ScopedCheckpointJsonDecode original_decode(
        CheckpointWorldJsonSection::household_purchases);
    auto purchase = decode_household_purchases(
        field(source, "household_purchases", "state"),
        catalog.selected_household_version);
    if (!purchase.ok)
      return {false, {}, std::move(purchase.error)};
    result.household_purchases =
        checkpoint_make_shared<const HouseholdPurchaseState>(
            std::move(purchase.value));
  }
  {
    ScopedCheckpointJsonDecode original_decode(
        CheckpointWorldJsonSection::household_retail_settlements);
    auto retail = decode_household_retail_state(
        field(source, "household_retail_settlements", "state"));
    if (!retail.ok)
      return {false, {}, std::move(retail.error)};
    result.household_retail_settlements =
        checkpoint_make_shared<const HouseholdRetailState>(
            std::move(retail.value));
  }
} else if (source.find("household_purchases") ||
           source.find("household_retail_settlements"))
  fail("UnexpectedHouseholdState", "state");
