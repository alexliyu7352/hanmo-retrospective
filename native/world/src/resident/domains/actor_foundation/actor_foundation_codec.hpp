#include <string_view>
// 三个原逻辑段的成对typed访客；这里只提供域叶，不另设世界存档或注册号。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class ProjectionWriter;

// 由已准入的世界profile显式选择；旧两个实验profile的正常字节永久保原序。
enum class ConditionWireProfile {
  legacy_rest_only,
  receipt_v2_union,
  capability_application_chain,
  purpose_capacity_transition,
  injury_effect_sources,
  named_custody_availability,
  trade_grant_capability_chain,
  injury_source_chain
};
Status visit_actor_start_context_v2(CodecVisitor &, ActorStartContextStateV2 &);
Status visit_character_origin_records_v1(
    CodecVisitor &, CharacterOriginRecordsStateV1 &,
    ActorOriginWireProfile = ActorOriginWireProfile::authored_only);
Status visit_character_condition_extensions_v1(
    CodecVisitor &, CharacterConditionExtensionsStateV1 &,
    ConditionWireProfile = ConditionWireProfile::legacy_rest_only);
Status visit_frozen_rest_capability_inputs(CodecVisitor &,
                                           FrozenFacilityCapabilityInputs &);
// 成对JSON只用于原冷World段；所有字段由相同typed visitor穷尽。
Outcome<BoundaryValue>
encode_actor_start_context_v2(const ActorStartContextStateV2 &);
Outcome<ActorStartContextStateV2>
decode_actor_start_context_v2(const BoundaryValue &);
Outcome<BoundaryValue> encode_character_origin_records_v1(
    const CharacterOriginRecordsStateV1 &,
    ActorOriginWireProfile = ActorOriginWireProfile::authored_only);
Outcome<CharacterOriginRecordsStateV1> decode_character_origin_records_v1(
    const BoundaryValue &,
    ActorOriginWireProfile = ActorOriginWireProfile::authored_only);
Outcome<BoundaryValue> encode_character_condition_extensions_v1(
    const CharacterConditionExtensionsStateV1 &,
    ConditionWireProfile = ConditionWireProfile::legacy_rest_only);
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v1(
    const BoundaryValue &,
    ConditionWireProfile = ConditionWireProfile::legacy_rest_only);
Outcome<BoundaryValue>
encode_frozen_rest_capability_inputs(const FrozenFacilityCapabilityInputs &);
Outcome<FrozenFacilityCapabilityInputs>
decode_frozen_rest_capability_inputs(const BoundaryValue &);
// 仅本叶验证工具的完整封套，正式World codec逐段调用上方三个visitor。
Outcome<CodecVisitor::Buffer> encode_actor_foundation_checkpoint(
    const AuthoredActorFoundationState &,
    ConditionWireProfile = ConditionWireProfile::legacy_rest_only);
Outcome<AuthoredActorFoundationState> decode_actor_foundation_checkpoint(
    const CodecVisitor::Buffer &,
    ConditionWireProfile = ConditionWireProfile::legacy_rest_only);
Status
emit_frozen_rest_capability_projection(ProjectionWriter &, std::string_view,
                                       const FrozenFacilityCapabilityInputs &);
Status emit_character_condition_extensions_v1_projection(
    ProjectionWriter &, std::string_view,
    const CharacterConditionExtensionsStateV1 &, ConditionWireProfile profile);
Status emit_character_origin_records_v1_projection(
    ProjectionWriter &, std::string_view, const CharacterOriginRecordsStateV1 &,
    ActorOriginWireProfile = ActorOriginWireProfile::authored_only);
Status emit_actor_start_context_v2_projection(ProjectionWriter &,
                                              std::string_view,
                                              const ActorStartContextStateV2 &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 仅完整Catalog/profile选用；旧rest-only入口仍原seed-only字段布局。
Status
visit_frozen_character_capability_inputs(CodecVisitor &,
                                         FrozenFacilityCapabilityInputs &);
Status emit_frozen_character_capability_projection(
    ProjectionWriter &, std::string_view,
    const FrozenFacilityCapabilityInputs &);
Outcome<BoundaryValue> encode_frozen_character_capability_inputs(
    const FrozenFacilityCapabilityInputs &);
Outcome<FrozenFacilityCapabilityInputs>
decode_frozen_character_capability_inputs(const BoundaryValue &);
} // namespace hanmo::resident

namespace hanmo::resident {
Status visit_actor_condition_head(CodecVisitor &, ActorConditionHead &);
Status emit_actor_condition_head_projection(ProjectionWriter &,
                                            std::string_view,
                                            const ActorConditionHead &);
Outcome<BoundaryValue> encode_actor_condition_head(const ActorConditionHead &);
Outcome<ActorConditionHead> decode_actor_condition_head(const BoundaryValue &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Selected only by the trusted actor_start_context/3 module registration.
// /2 remains byte-identical and rejects dynamic seed sources on write.
Status visit_actor_start_context_v3(CodecVisitor &, ActorStartContextStateV2 &);
Status emit_actor_start_context_v3_projection(ProjectionWriter &,
                                              std::string_view,
                                              const ActorStartContextStateV2 &);
Outcome<BoundaryValue>
encode_actor_start_context_v3(const ActorStartContextStateV2 &);
Outcome<ActorStartContextStateV2>
decode_actor_start_context_v3(const BoundaryValue &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Explicit physical /3. The existing /1 and community-wrapper /2 are unchanged.
Status
visit_character_condition_extensions_v3(CodecVisitor &,
                                        CharacterConditionExtensionsStateV1 &);
Outcome<BoundaryValue> encode_character_condition_extensions_v3(
    const CharacterConditionExtensionsStateV1 &);
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v3(const BoundaryValue &);
Status emit_character_condition_extensions_v3_projection(
    ProjectionWriter &, std::string_view,
    const CharacterConditionExtensionsStateV1 &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 嵌入原 Care/设施 worker frame，只发当前 object 字段，同完整 capability
// visitor。
Status
emit_frozen_character_capability_fields(ProjectionWriter &,
                                        const FrozenFacilityCapabilityInputs &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Explicit physical /4. Original /1-/3 retain their prior cause bytes.
Status
visit_character_condition_extensions_v4(CodecVisitor &,
                                        CharacterConditionExtensionsStateV1 &);
Outcome<BoundaryValue> encode_character_condition_extensions_v4(
    const CharacterConditionExtensionsStateV1 &);
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v4(const BoundaryValue &);
Status emit_character_condition_extensions_v4_projection(
    ProjectionWriter &, std::string_view,
    const CharacterConditionExtensionsStateV1 &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Explicit physical /5: immutable Applied results and original typed roots.
// This codec is not a source validator or a factory admission decision.
Status
visit_character_condition_extensions_v5(CodecVisitor &,
                                        CharacterConditionExtensionsStateV1 &);
Outcome<BoundaryValue> encode_character_condition_extensions_v5(
    const CharacterConditionExtensionsStateV1 &);
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v5(const BoundaryValue &);
Status emit_character_condition_extensions_v5_projection(
    ProjectionWriter &, std::string_view,
    const CharacterConditionExtensionsStateV1 &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 显式物理6沿4，不选择5的伤病effect表；旧1–5的字节与准入不变。
Status
visit_character_condition_extensions_v6(CodecVisitor &,
                                        CharacterConditionExtensionsStateV1 &);
Outcome<BoundaryValue> encode_character_condition_extensions_v6(
    const CharacterConditionExtensionsStateV1 &);
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v6(const BoundaryValue &);
Status emit_character_condition_extensions_v6_projection(
    ProjectionWriter &, std::string_view,
    const CharacterConditionExtensionsStateV1 &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 显式物理 /7 仅沿原 /3 加 trade-grant 来源；不启用 /4-/6 的后置能力。
Status visit_character_condition_extensions_v7(CodecVisitor&,CharacterConditionExtensionsStateV1&);
Status emit_character_condition_extensions_v7_projection(ProjectionWriter&,std::string_view,const CharacterConditionExtensionsStateV1&);
Outcome<BoundaryValue> encode_character_condition_extensions_v7(const CharacterConditionExtensionsStateV1&);
Outcome<CharacterConditionExtensionsStateV1> decode_character_condition_extensions_v7(const BoundaryValue&);
}

namespace hanmo::resident {
// Explicit physical /8: /7 capability wire plus the independent source/1
// reference and verified injury effects. Old /1..7 never accept its new cause.
// This does not select /4 purpose transitions or /6 named custody.
Status visit_character_condition_extensions_v8(CodecVisitor&,CharacterConditionExtensionsStateV1&);
Status emit_character_condition_extensions_v8_projection(ProjectionWriter&,std::string_view,const CharacterConditionExtensionsStateV1&);
Outcome<BoundaryValue> encode_character_condition_extensions_v8(const CharacterConditionExtensionsStateV1&);
Outcome<CharacterConditionExtensionsStateV1> decode_character_condition_extensions_v8(const BoundaryValue&);
}
