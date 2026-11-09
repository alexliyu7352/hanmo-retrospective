// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = validate_native_private_transport_funding_world(w);
if (!r.ok)
  return r;
if (w.catalog->private_transport_funding_selected) {
  r = section("private_transport_funding", [&](auto &c) {
    return visit_private_transport_funding_checkpoint(
        c, *w.private_transport_funding);
  });
  if (!r.ok)
    return r;
}
