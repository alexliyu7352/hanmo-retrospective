// 事务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 债务需求只给显式新目录单独格式；旧库存需求银行不能被自动升级。
template <class DomainList> DomainList obligation_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "affairs")
      domain.codec_version = 2;
  return domains;
}
bool obligation_needs_enabled(const PreparedCatalog &catalog) {
  return catalog.affairs && catalog.affairs->obligation_needs;
}
// 财政需求独立于工资开关，仅新作者显式选择Affairs3。
bool fiscal_needs_enabled(const PreparedCatalog &catalog) {
  return catalog.affairs && bool(catalog.affairs->fiscal_need_policies);
}
template <class DomainList> DomainList fiscal_need_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "affairs")
      domain.codec_version = 3;
  return domains;
}
// 匿名工资是独立显式映射，旧Affairs1/2/3载荷不变。
bool cohort_wage_needs_enabled(const PreparedCatalog &catalog) {
  return catalog.affairs && bool(catalog.affairs->cohort_wage_need_policies);
}
template <class DomainList>
DomainList cohort_wage_need_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "affairs")
      domain.codec_version = 4;
  return domains;
}
