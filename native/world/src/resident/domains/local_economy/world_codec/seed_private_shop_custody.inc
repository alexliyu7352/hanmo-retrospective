// 明确选型的唯一原源 JSON 段；缺段和未选多段均拒。
auto private_custody =
    selected_native_private_shop_household_custody_version(catalog.identity);
if (!private_custody.ok)
  return {false, {}, private_custody.error};
if (private_custody.value) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::private_shop_household_custody);
  auto decoded = decode_private_shop_household_custody_values(
      field(source, "private_shop_household_custody_sources", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result.private_shop_household_custody =
      checkpoint_make_shared<const PrivateShopHouseholdCustodyState>(
          std::move(decoded.value));
} else if (source.find("private_shop_household_custody_sources"))
  return common_failure("UnexpectedPrivateShopHouseholdCustodyState");
