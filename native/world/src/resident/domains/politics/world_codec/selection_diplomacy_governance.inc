// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
template <class DomainList>
DomainList diplomacy_domains(DomainList domains, Integer version = 1) {
  domains.push_back({"diplomacy", static_cast<std::uint32_t>(version)});
  domains.push_back({"envoy_tasks", static_cast<std::uint32_t>(version)});
  return domains;
}
// 三域治理只整体附加一种组合，不派生部分启用的八种排列。
template <class DomainList> DomainList governance_domains(DomainList domains) {
  for (const auto *name :
       {"politics_governance", "governance_tasks", "public_relief"})
    domains.push_back({name, 1});
  return domains;
}
