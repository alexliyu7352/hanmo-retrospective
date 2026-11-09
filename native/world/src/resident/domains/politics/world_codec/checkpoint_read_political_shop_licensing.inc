// 政治检查点恢复分派：沿原 else-if 链读取 political_shop_licensing,
// political_shop_registry_successions；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "political_shop_licensing") {
  auto selected =
      selected_native_political_shop_licensing_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || section.codec_version != selected.value ||
      !catalog.political_shop_licensing)
    return {false,
            {},
            common_failure("PoliticalShopLicensingCheckpointSelectionMismatch")
                .error};
  auto state = make_state<Seed, PoliticalShopLicensingState>();
  r = selected.value == 2
          ? visit_political_shop_licensing_checkpoint2(reader, *state)
          : visit_political_shop_licensing_checkpoint(reader, *state);
  seed.political_shop_licensing = std::move(state);
}
else if (section.domain == "political_shop_registry_successions") {
  auto state = make_state<Seed, PoliticalShopRegistrySuccessionState>();
  r = visit_political_shop_registry_successions_checkpoint(reader, *state);
  seed.political_shop_registry_successions = std::move(state);
}