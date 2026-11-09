// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 管理与家庭自养各有独立marker；codec5不隐式代表两个可选块都存在。
bool management_enabled(const PreparedCatalog &c) {
  return c.local_economy && c.local_economy->production_management &&
         c.local_economy->production_management->version == 1;
}
bool household_management_enabled(const PreparedCatalog &c) {
  return c.household_provision_management &&
         c.household_provision_management->version == 1;
}
bool disruption_enabled(const PreparedCatalog &c) {
  return c.production_disruptions && c.production_disruptions->version == 1;
}
