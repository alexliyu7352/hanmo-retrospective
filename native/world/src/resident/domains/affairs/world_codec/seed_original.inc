// 事务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto affairs = decode_affairs_state(
    boundary::field(source, "affairs", ""), catalog.affairs->obligation_needs,
    fiscal_needs_enabled(catalog), cohort_wage_needs_enabled(catalog));
if (!affairs.ok)
  return {false, {}, affairs.error};
result.affairs = std::make_shared<const AffairsState>(std::move(affairs.value));
