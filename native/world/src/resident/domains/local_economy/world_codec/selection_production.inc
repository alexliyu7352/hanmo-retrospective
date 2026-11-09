// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
bool household_enabled(const PreparedCatalog &c) {
  return c.local_economy && c.local_economy->household_provisioning &&
         c.local_economy->household_provisioning->version == 1;
}
bool relief_enabled(const PreparedCatalog &c) {
  return c.governance_tasks_enabled;
}
bool settlement_enabled(const PreparedCatalog &c) {
  return c.fiscal_settlement && c.fiscal_settlement->version == 1;
}
template <class DomainList>
DomainList economic_domains(DomainList domains, Integer version) {
  for (auto &d : domains)
    if (d.name == "local_economy")
      d.codec_version = version;
  return domains;
}
