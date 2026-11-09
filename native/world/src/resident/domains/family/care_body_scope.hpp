// FH §5.3A 的原身体份额深值，只描述一次真实清场前后的范围。
// 本值没有人口、当前身体或空间写权限，形状检查不能证明已经移走任何人。
#pragma once
#include "death_ref.hpp"
#include "resident/domains/actor_foundation/condition_refs.hpp"
#include "resident/kernel/world_timeline.hpp"

namespace hanmo::resident {
// 零身体份额的唯一分支；死亡不能把具名或匿名身体自动改成此值。
struct CareEmptyBodyScope {};

// 一个真实具名身体，Origin 使用原 actor 稳定键，不伪造 Common 来源。
struct CareNamedBodyScope {
  StableId actor_id, character_origin_actor_key;
  CharacterConditionSourceRef condition_basis;
  std::optional<DeathRef> death_ref;
};

// 原住居 line 的实际份额；已知具名成员只是该份额的子集，不另加人数。
struct CareResidenceBodyScope {
  StableId agreement_id, intake_line_id;
  SourceRef actual_scope_source_ref;
  Integer actual_scope_schema = 0, body_count = 0;
  std::vector<StableId> known_named_actor_ids;
  std::vector<MomentOwnerSource> unresolved_loss_source_refs;
};

// 原 wire tag 固定 Empty=0、Named=1、ResidenceSlice=2，不开放任意对象分支。
using CareBodyScopeSlice = std::variant<CareEmptyBodyScope, CareNamedBodyScope,
                                        CareResidenceBodyScope>;

class CodecVisitor;
class ProjectionWriter;
// 原 binary/state visitor 共用同一字段序，不改变真实占位或人口。
Status visit_care_body_scope(CodecVisitor &, CareBodyScopeSlice &);
// 输出原完整份额，不按当下人数/生命状态补历史。
Status emit_care_body_scope_fields(ProjectionWriter &,
                                   const CareBodyScopeSlice &);
// 仅创建序列化边界值，不作为长期自由字段业务存储。
Outcome<BoundaryValue> encode_care_body_scope(const CareBodyScopeSlice &);
// 未知 tag、坏引用、重复具名子集或不合法数量均拒绝，不以空值兜底。
Outcome<CareBodyScopeSlice> decode_care_body_scope(const BoundaryValue &);
} // namespace hanmo::resident
