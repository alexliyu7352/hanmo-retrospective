// 决策世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 主体登记只提升显式启用的决策域；旧三种决策格式和其他领域版本不变。
template <class DomainList> DomainList registry_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "director")
      domain.codec_version = 4;
  return domains;
}
bool registry_enabled(const PreparedCatalog &catalog) {
  return catalog.director && catalog.director->dynamic_subjects;
}
