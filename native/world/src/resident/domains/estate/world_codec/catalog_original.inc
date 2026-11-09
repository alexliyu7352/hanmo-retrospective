// 庄园世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto e = [&]() {
  ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::estate);
  return decode_estate_catalog(data, *result->common);
}();
if (!e.ok)
  return {false, {}, e.error};
result->estate =
    checkpoint_make_shared<const EstateCatalog>(std::move(e.value));
