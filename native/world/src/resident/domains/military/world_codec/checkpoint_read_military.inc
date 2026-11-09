// 军事检查点恢复分派：沿原 else-if 链读取 military, military_battle_lifecycle,
// military_naval, siege_integration,
// military_integration；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "military") {
  if (military_body_decoded)
    return {
        false, {}, common_failure("DuplicateMilitaryOriginalCheckpoint").error};
  r = visit_military_checkpoint(reader, *m,
                                catalog.selected_military_occupation_schema);
  if (r.ok)
    military_body_decoded = true;
}
else if (section.domain == "military_battle_lifecycle") {
  auto selected = selected_native_military_battle_lifecycle(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value != 1 || section.codec_version != 1 ||
      !military_body_decoded || military_lifecycle_decoded)
    return {false,
            {},
            common_failure("MilitaryBattleLifecycleCheckpointSelectionMismatch")
                .error};
  r = visit_military_battle_lifecycle(reader, *m, selected.value);
  if (r.ok)
    military_lifecycle_decoded = true;
}
else if (section.domain == "military_naval") {
  if (!catalog.naval)
    return {false, {}, common_failure("UnexpectedNavalState").error};
  auto n = make_state<Seed, NavalState>();
  r = naval_codec::visit(reader, *n, *catalog.naval);
  seed.naval = std::move(n);
}
else if (section.domain == "siege_integration") r =
    visit_siege_integration_checkpoint(reader, m->siege_integration);
else if (section.domain == "military_integration") r =
    visit_military_integration_facts(reader, m->integration_facts);
