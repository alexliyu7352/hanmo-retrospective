// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto shop_operations = validate_native_private_shop_operation_world(w);
if (!shop_operations.ok)
  return shop_operations;
if (w.private_shop_operation_sources)
  WORLD_FIELDS(P::section, "private_shop_operation_sources",
               emit_private_shop_operation_sources_fields(
                   out, *w.private_shop_operation_sources));
if (w.private_shop_operation_capabilities)
  WORLD_FIELDS(P::section, "private_shop_operation_capabilities",
               emit_private_shop_operation_capabilities_fields(
                   out, *w.private_shop_operation_capabilities));
