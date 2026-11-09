// 独占原 decoded seed；只接受已选版本1，不合并重复段。
else if (section.domain == "private_shop_household_custody_sources") {
  auto selected =
      selected_native_private_shop_household_custody_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (selected.value != 1 || section.codec_version != 1 ||
      seed.private_shop_household_custody)
    return {
        false,
        {},
        common_failure("PrivateShopCustodyCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, PrivateShopHouseholdCustodyState>();
  r = visit_private_shop_household_custody_values(reader, *state);
  seed.private_shop_household_custody = std::move(state);
}
