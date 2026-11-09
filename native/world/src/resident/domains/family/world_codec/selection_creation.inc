// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
bool adult_adoption_enabled(const PreparedCatalog &c) {
  return c.family && c.family->adult_adoption.has_value();
}
template <class DomainList>
DomainList adult_adoption_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "family") {
      if (d.codec_version < 1 || d.codec_version > 4)
        boundary::fail("AdultAdoptionCheckpointProfileRequired",
                       "checkpoint/family");
      d.codec_version += 4;
    }
  return domains;
}
bool character_creation_enabled(const PreparedCatalog &c) {
  return c.family && c.family->character_creation.has_value();
}
template <class DomainList>
DomainList character_creation_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "family") {
      if (d.codec_version != 1 && d.codec_version != 2)
        boundary::fail("CharacterCreationCheckpointProfileRequired",
                       "checkpoint/family");
      d.codec_version += 2;
    }
  return domains;
}
bool household_control_enabled(const PreparedCatalog &c) {
  return c.family && c.family->household_control.has_value();
}
template <class DomainList>
DomainList household_control_domains(DomainList domains) {
  for (auto &d : domains)
    if (d.name == "family")
      d.codec_version = 2;
  return domains;
}
