// 商业检查点恢复分派：沿原 else-if 链读取
// household_retail_settlements；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "household_retail_settlements") {
  if (!catalog.household_retail_settlements || section.codec_version != 1)
    return {false,
            {},
            common_failure("HouseholdCheckpointSelectionMismatch").error};
  auto retail = make_state<Seed, HouseholdRetailState>();
  r = visit_household_retail_checkpoint(reader, *retail);
  seed.household_retail_settlements = std::move(retail);
}