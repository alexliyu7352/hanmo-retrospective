// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto funding = validate_native_private_transport_funding_world(w);
if (!funding.ok)
  return funding;
if (w.catalog->private_transport_funding_selected)
  WORLD_WHOLE(P::section, "private_transport_funding",
              emit_private_transport_funding_projection(
                  out, "", *w.private_transport_funding));
