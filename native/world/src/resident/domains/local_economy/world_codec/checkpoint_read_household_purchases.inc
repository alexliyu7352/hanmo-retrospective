// 地方经济检查点恢复分派：沿原 else-if 链读取
// household_purchases；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "household_purchases") {
  if (!catalog.household_purchases ||
      section.codec_version != catalog.selected_household_version)
    return {false,
            {},
            common_failure("HouseholdCheckpointSelectionMismatch").error};
  auto purchase = make_state<Seed, HouseholdPurchaseState>();
  r = visit_household_purchases(reader, *purchase,
                                catalog.selected_household_version);
  seed.household_purchases = std::move(purchase);
}