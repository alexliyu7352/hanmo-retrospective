// 事务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = section("affairs", [&](auto &c) {
  return visit_affairs_checkpoint(
      c, w.affairs, w.catalog->affairs->obligation_needs,
      fiscal_needs_enabled(*w.catalog), cohort_wage_needs_enabled(*w.catalog));
});
if (!r.ok)
  return {false, {}, r.error};
