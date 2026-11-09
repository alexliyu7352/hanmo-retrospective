// 行旅世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 原world模板选择只读place.scene_template_ids.world，再读当前地图单模板；别的视图映射不预取。
const auto template_value = [](const BoundaryValue *value) {
  if (!value)
    return WorldTemplateMapping::absent;
  const auto *id = value->as<StableId>();
  return id && !id->empty() ? WorldTemplateMapping::present
                            : WorldTemplateMapping::invalid;
};
for (const auto &row : object(field(data, "places", "data"), "data/places")) {
  auto selected = WorldTemplateMapping::absent;
  if (const auto *mapping = row.second.find("scene_template_ids"))
    selected = mapping->as<BoundaryValue::Dict>()
                   ? template_value(mapping->find("world"))
                   : WorldTemplateMapping::invalid;
  result->place_world_templates.emplace_back(row.first, selected);
}
if (const auto *maps = data.find("world_maps"))
  if (const auto *map = maps->find(result->travel->world_map_id))
    result->world_template = template_value(map->find("scene_template_id"));
