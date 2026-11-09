// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = validate_native_private_shop_operation_world(w);
if (!r.ok)
  return r;
if (w.private_shop_operation_sources) {
  r = section("private_shop_operation_sources", [&](auto &c) {
    return visit_private_shop_operation_sources(
        c, *w.private_shop_operation_sources);
  });
  if (!r.ok)
    return r;
}
if (w.private_shop_operation_capabilities) {
  r = section("private_shop_operation_capabilities", [&](auto &c) {
    return visit_private_shop_operation_capabilities(
        c, *w.private_shop_operation_capabilities);
  });
  if (!r.ok)
    return r;
}
