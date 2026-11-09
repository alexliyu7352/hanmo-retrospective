// 成长检查点恢复分派：沿原 else-if 链读取
// growth_permanent_contributions；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "growth_permanent_contributions") {
  if (!catalog.growth->complete_capability_application_chain ||
      section.codec_version != 3 || seed.growth_permanent ||
      seed.card_acquisitions)
    return {false,
            {},
            common_failure("GrowthPermanentCheckpointProfileMismatch").error};
  auto permanent = make_state<Seed, GrowthPermanentState>();
  auto cards = make_state<Seed, CardAcquisitionState>();
  r = visit_growth_permanent_section(
      reader, *permanent, *cards,
      GrowthPermanentSectionVersion::trade_grants);
  seed.growth_permanent = std::move(permanent);
  seed.card_acquisitions = std::move(cards);
}