// 职责：拥有原 Military 物理定义的唯一不可变目录表。
// 只承载已有八槽记录，不生成 chart、入场事实或 Session definition lock。
#pragma once

#include "military_spatial_chart.hpp"

namespace hanmo::resident {
struct BoundaryValue;

// 职责：按原 definition id 保存作者明确提供的完整记录。
// 空表保留真实空选择；任何实际查找仍须找到原行，不能返回默认物理参数。
struct MilitarySpatialDefinitionCatalog {
  OrderedTable<MilitarySpatialGeometryDefinition2> definitions;
};

// 职责：沿原有 JSON 冷读预算和 geometry record codec 解码键表。
// 拒绝第二种数组形状、重复键、键值不等和原记录的非法几何。
Outcome<MilitarySpatialDefinitionCatalog>
decode_military_spatial_definition_catalog(const BoundaryValue &);

// 职责：核对目录键及原 geometry validator，不重写其数学或失败语义。
// 外部地图、入口授权和真实接触来源仍由其原 owner 核验。
Status validate_military_spatial_definition_catalog(
    const MilitarySpatialDefinitionCatalog &, ExactArithmeticBudget = {});

// 职责：借用同一不可变目录中的真实原行，不复制定义或推导缺失来源。
// 未找到 id 时准确拒绝；返回指针的生命期不得超过该目录 owner。
Outcome<const MilitarySpatialGeometryDefinition2 *>
read_military_spatial_definition(const MilitarySpatialDefinitionCatalog &,
                                 const StableId &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
