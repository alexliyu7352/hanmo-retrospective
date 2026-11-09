// 原形态要求的锁内目录叶，不新增 World 段或可单独发布的 continuity 能力。
#pragma once
#include "political_foundation_terms.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
// 全表只核实际原键和三字段；允许目录无实例，真正业务读缺行必须拒绝。
Status validate_foundation_requirement_catalog(
    const PoliticalFoundationRequirementCatalog &);
Status
visit_foundation_requirement_catalog(CodecVisitor &,
                                     PoliticalFoundationRequirementCatalog &);
// 同一原字段表的字段级出口，只供拥有原对象的组合codec使用。
Status emit_foundation_requirement_catalog_fields(
    ProjectionWriter &, const PoliticalFoundationRequirementCatalog &);
Status emit_foundation_requirement_catalog_projection(
    ProjectionWriter &, std::string_view,
    const PoliticalFoundationRequirementCatalog &);
Outcome<BoundaryValue> encode_foundation_requirement_catalog(
    const PoliticalFoundationRequirementCatalog &);
Outcome<PoliticalFoundationRequirementCatalog>
decode_foundation_requirement_catalog(
    const BoundaryValue &, const CatalogIdentity &actual_locked_identity);
} // namespace hanmo::resident
