// 制造世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  auto headers = validate_native_item_craft_original_headers(
      w.common, *w.catalog, w.item_instances ? &*w.item_instances : nullptr,
      w.craft_jobs ? &*w.craft_jobs : nullptr);
  if (!headers.ok)
    return headers;
}
