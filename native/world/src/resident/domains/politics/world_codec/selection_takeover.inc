// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
bool treaty_passage_enabled(const PreparedCatalog &c) {
  return c.treaty_passage && c.treaty_passage->enabled;
}
bool political_military_appointments_enabled(const PreparedCatalog &c) {
  return c.political_military_appointments &&
         c.political_military_appointments->enabled;
}
bool political_field_appointments_enabled(const PreparedCatalog &c) {
  return c.political_field_appointments &&
         c.political_field_appointments->enabled;
}
bool occupation_fiscal_enabled(const PreparedCatalog &c) {
  return c.occupation_fiscal && c.occupation_fiscal->enabled;
}
template <class DomainList>
DomainList occupation_fiscal_domains(DomainList domains) {
  domains.push_back({"political_occupation_fiscal", 1});
  domains.push_back({"occupation_fiscal_settlement", 1});
  return domains;
}
bool takeover_enabled(const PreparedCatalog &c) {
  return c.political_takeover && c.political_takeover->enabled;
}
template <class DomainList>
DomainList takeover_domains(DomainList domains, Integer task_version) {
  for (const auto *name : {"political_takeover", "economic_takeover_execution"})
    domains.push_back({name, 1});
  domains.push_back(
      {"task_takeover_execution", static_cast<std::uint32_t>(task_version)});
  return domains;
}
