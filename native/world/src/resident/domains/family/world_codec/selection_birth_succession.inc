// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
bool family_birth_enabled(const PreparedCatalog &c) {
  return c.family_birth && c.family_birth->enabled;
}
// 志愿接手独立尾段，旧出生与Family格式不加空表。
bool family_care_relief_enabled(const PreparedCatalog &c) {
  return c.family_care_relief && c.family_care_relief->enabled;
}
bool voluntary_succession_enabled(const PreparedCatalog &c) {
  return c.family && c.family->voluntary_succession.has_value();
}
template <class DomainList>
DomainList voluntary_succession_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "family") {
      if (d.codec_version < 1 || d.codec_version > 8)
        boundary::fail("VoluntarySuccessionCheckpointProfileRequired",
                       "checkpoint/family");
      d.codec_version += 8;
    }
  return domains;
}
// 原Family段的显式组合版号统一由有限helper选择；不从Common位猜新生前路径。
Integer family_version_for(const PreparedCatalog &catalog) {
  auto selected = select_family_codec_version(
      {household_control_enabled(catalog), character_creation_enabled(catalog),
       adult_adoption_enabled(catalog), voluntary_succession_enabled(catalog),
       bool(catalog.family_voluntary_paths)});
  if (!selected.ok)
    boundary::fail(selected.error.original.code, "checkpoint/family");
  return selected.value;
}
// 标题与完整读共用同一个有限pair，重复/未知marker仍拒，不把格式ID当新控制来源。
Status
family_modules_for(Integer version,
                   const std::vector<std::pair<StableId, StableId>> &modules) {
  std::optional<Integer> voluntary, paths, known;
  for (const auto &marker : modules) {
    std::optional<Integer> *value =
        marker.first == "voluntary_succession"       ? &voluntary
        : marker.first == "family_voluntary_paths"   ? &paths
        : marker.first == "known_personal_dialogues" ? &known
                                                     : nullptr;
    if (!value)
      continue;
    if (*value || (marker.second != "1" &&
                   !(value == &voluntary && marker.second == "2")))
      return common_failure("FamilyVoluntaryCodecModuleMismatch", marker.first);
    *value = marker.second == "2" ? 2 : 1;
  }
  return validate_family_codec_module_pair(version, voluntary, paths, known);
}
