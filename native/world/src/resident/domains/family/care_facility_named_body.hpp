// FH §3 的局部具名身体内容；只引用原 owner，不新增身体或生命拥有账。
#pragma once
#include "death_ref.hpp"
#include "resident/domains/actor_foundation/condition_refs.hpp"

namespace hanmo::resident {
// 三支原来源互斥；死亡仍是具名身体，不能自动成为空占位。
using CareFacilityBodyBasisRef =
    std::variant<InitialConditionSeedRef, ConditionReceiptRefV2, DeathRef>;

// 原设施事件的三字段历史深值，不复用清场份额的不同保存布局。
struct CareFacilityNamedBody {
  StableId actor_id;
  CareFacilityBodyBasisRef body_basis_ref;
  Integer body_basis_schema = 0;
};

class CodecVisitor;
class ProjectionWriter;
// 原 binary visitor 共用唯一字段表；只核形状，不证明真实入位或来源已采用。
Status visit_care_facility_named_body(CodecVisitor &, CareFacilityNamedBody &);
// 投影原历史三字段，不从当前生命状态补值或替换来源。
Status emit_care_facility_named_body_fields(ProjectionWriter &,
                                            const CareFacilityNamedBody &);
// 只创建序列化边界值，不形成第二身体表。
Outcome<BoundaryValue>
encode_care_facility_named_body(const CareFacilityNamedBody &);
// 严拒未知分支、坏原引用及非正 schema；原 owner 的真实来源校验另行完成。
Outcome<CareFacilityNamedBody>
decode_care_facility_named_body(const BoundaryValue &);
} // namespace hanmo::resident
