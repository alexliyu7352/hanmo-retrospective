#pragma once
#include "treaty_aid_resource_proposal2.hpp"
namespace hanmo::resident {
// 最终接受源属于原 selected /2 条约体。量、路线与最大暴露仍借不可变条款和
// 原选定提案，不复制第二套数值，也不允许后生 Schedule 倒造接受源。
struct TreatyAidAcceptedResourceSource2 {
  StableId id, term_id;
  MomentOwnerSource activation, proposal, recipient_consent;
  WorldMoment accepted_at;
};
SourceRef
treaty_aid_accepted_resource_source2(const TreatyAidAcceptedResourceSource2 &);
Status validate_treaty_aid_accepted_resource_source2_shape(
    const TreatyAidAcceptedResourceSource2 &);
Status
visit_treaty_aid_accepted_resource_source2(CodecVisitor &,
                                           TreatyAidAcceptedResourceSource2 &);
Status emit_treaty_aid_accepted_resource_source2_projection(
    ProjectionWriter &, std::string_view,
    const TreatyAidAcceptedResourceSource2 &);
Outcome<BoundaryValue> encode_treaty_aid_accepted_resource_source2(
    const TreatyAidAcceptedResourceSource2 &);
Outcome<TreatyAidAcceptedResourceSource2>
decode_treaty_aid_accepted_resource_source2(const BoundaryValue &);
} // namespace hanmo::resident
