// 政治检查点恢复分派：沿原 else-if 链读取 treaty_passage, diplomacy,
// politics_governance, politics,
// political_occupation_fiscal；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "treaty_passage") {
  auto treaty = make_state<Seed, TreatyPassageState>();
  r = visit_treaty_passage_checkpoint(reader, *treaty);
  seed.treaty_passage = std::move(treaty);
}
else if (section.domain == "diplomacy") {
  auto diplomatic = make_state<Seed, DiplomacyState>();
  r = visit_diplomacy_checkpoint(reader, *diplomatic);
  seed.diplomacy = std::move(diplomatic);
}
else if (section.domain == "politics_governance") {
  auto governance = make_state<Seed, PoliticsGovernanceState>();
  r = visit_politics_governance_checkpoint(reader, *governance);
  seed.politics_governance = std::move(governance);
}
else if (section.domain == "politics") {
  auto selected =
      read_political_locked_body_version(catalog.identity, "politics");
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || section.codec_version != selected.value ||
      !politics_enabled(catalog))
    return {false,
            {},
            common_failure("PoliticalCheckpointSelectionMismatch").error};
  r = selected.value == 2 ? visit_politics2(reader, *politics)
                          : visit_politics_checkpoint(reader, *politics);
}
else if (section.domain == "political_occupation_fiscal") {
  auto state = make_state<Seed, OccupationFiscalState>();
  r = visit_occupation_fiscal_checkpoint(reader, *state);
  seed.occupation_fiscal = std::move(state);
}