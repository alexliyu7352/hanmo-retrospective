// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 独立恢复payload在本段；Common只持精确双标签头，未知/孤儿/错域不能丢弃。
{
  std::set<StableId> found;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *domain = row.second.find("domain");
    const bool own = kind == "personal_recovery_credit" ||
                     kind == "personal_recovery_closed";
    if (!own &&
        (!domain || string(*domain, row.first) != "personal_injury_recovery"))
      continue;
    if (!catalog.personal_recovery || !result.personal_recovery || !own ||
        !domain || string(*domain, row.first) != "personal_injury_recovery")
      fail("PersonalInjuryRecoveryProfileMismatch", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    const bool exists =
        kind == "personal_recovery_credit"
            ? result.personal_recovery->credits.find(row.first) != nullptr
            : result.personal_recovery->closures.find(row.first) != nullptr;
    if (!exists || object(row.second, row.first).size() != 2 ||
        !found.insert(row.first).second)
      fail("GrowthRecoveryFactMismatch", row.first);
  }
  if (result.personal_recovery &&
      found.size() != result.personal_recovery->credits.size() +
                          result.personal_recovery->closures.size())
    fail("GrowthRecoveryFactMismatch", "personal_injury_recovery");
}
