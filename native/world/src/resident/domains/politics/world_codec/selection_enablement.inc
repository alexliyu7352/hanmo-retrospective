// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
bool politics_enabled(const PreparedCatalog &c) {
  return c.politics && c.politics->enabled;
}
bool political_fiscal_enabled(const PreparedCatalog &c) {
  return c.political_fiscal && c.political_fiscal->enabled;
}
