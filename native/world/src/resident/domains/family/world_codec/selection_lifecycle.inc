// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 新同意账本只在显式profile下追加到旧家庭1..8，保旧字节合同。
// 独立生命段只由本情景显式启用；不会提升旧Family格式。
bool family_life_enabled(const PreparedCatalog &c) {
  return c.family_life && c.family_life->enabled;
}
// 独立许可段必须显式启用，不改变既有Family版本选择。
bool family_legacy_enabled(const PreparedCatalog &c) {
  return c.family_legacy && c.family_legacy->enabled;
}
// 遗赠另占显式新段，旧家庭与控制段字节不改。
bool family_bequest_enabled(const PreparedCatalog &c) {
  return c.family_bequest && c.family_bequest->enabled;
}
// 继产另占尾段，所有已有段及版本字节顺序保持。
bool family_inheritance_enabled(const PreparedCatalog &c) {
  return c.family_inheritance && c.family_inheritance->enabled;
}
// 婚议独立尾段，不改变既有Family1..16或任何前序格式。
bool family_marriage_enabled(const PreparedCatalog &c) {
  return c.family_marriage && c.family_marriage->enabled;
}
// 独立出生尾段与只读谱系目录marker成对，旧Family1..16不加空表。
