// 个人战斗检查点恢复分派：沿原 else-if 链读取 personal,
// personal_battle_lifecycle；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "personal") {
  if (personal_body_decoded || section.codec_version != 1)
    return {false,
            {},
            common_failure("PersonalOriginalCheckpointOrderMismatch").error};
  r = visit_personal_checkpoint(reader, *personal);
  if (r.ok)
    personal_body_decoded = true;
}
else if (section.domain == "personal_battle_lifecycle") {
  auto selected = selected_native_personal_battle_lifecycle(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value != 1 || section.codec_version != 1 ||
      !personal_body_decoded || personal_lifecycle_decoded)
    return {false,
            {},
            common_failure("PersonalBattleLifecycleCheckpointSelectionMismatch")
                .error};
  r = visit_personal_battle_lifecycle(reader, *personal, selected.value);
  if (r.ok)
    personal_lifecycle_decoded = true;
}