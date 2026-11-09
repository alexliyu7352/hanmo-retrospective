// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::local_economy);
  auto economy = decode_local_economy_state(
      boundary::field(source, "local_economy", ""), population_enabled(catalog),
      catalog.named_membership, management_enabled(catalog),
      household_enabled(catalog), obligation_versions.value.local_economy);
  if (!economy.ok)
    return {false, {}, economy.error};
  result.local_economy =
      checkpoint_make_shared<const LocalEconomyState>(std::move(economy.value));
}
