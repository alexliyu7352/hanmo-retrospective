// family_legacy/2专用亲代来源标签；不将Installed条目伪装成Common事实，也不另建控制账。
#pragma once
#include "biological_legacy_basis.hpp"
#include "collateral_family_origin_values.hpp"
namespace hanmo::resident {
// 仅从已核同候选原亲代/出生读取；没有明确直接亲代就拒绝，不读取任意当前关系标签。
Outcome<BiologicalControlOrigin>
make_biological_control_origin(const BiologicalLegacyBasisContext &,
                               const StableId &parent, const StableId &child);
// 实际原Legacy接受/撤回头前重读同一来源；年龄、生死、本人发言和控制决定仍由原业务校验。
Status validate_biological_control_origin_before_decision(
    const BiologicalLegacyBasisContext &, const BiologicalControlOrigin &,
    Integer tick, const StableId &actual_legacy_fact);
bool biological_control_origin_equal(const BiologicalControlOrigin &,
                                     const BiologicalControlOrigin &);
// 仅新格式显式调用；旧FamilyLegacy/1 visitor不添加字段或存在位。
Outcome<BiologicalControlOrigin>
decode_biological_control_origin(const BoundaryValue &);
BoundaryValue encode_biological_control_origin(const BiologicalControlOrigin &);
Status visit_biological_control_origin_checkpoint(CodecVisitor &,
                                                  BiologicalControlOrigin &);
} // namespace hanmo::resident
