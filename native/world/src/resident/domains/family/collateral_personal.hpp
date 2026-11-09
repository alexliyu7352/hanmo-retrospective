// v1e §2.2、§3.1、§7.1/7.2 的原旁支 source2；不修改旧生日/对白来源。
#pragma once
#include "care_education.hpp"
#include "collateral_family_origin.hpp"
#include "family_proposal_refs2.hpp"

namespace hanmo::resident {
// second 本人向 first 提出；原真实祖系只在这份不可变条款里拥有一次。
struct CollateralPersonalTerms2 {
  FamilyTermsTemplateRef template_ref;
  Integer terms_version = 1;
  StableId first_actor_id, second_actor_id;
  CollateralFamilyOrigin ancestry;
  FieldOrder field_order;
};

// 原 PersonalProposal2 的完整顺序。旁支无可协商项，因此两个 Optional
// 必须同时为空且 round=0；保原字段而不是削成另一种专用 wire。
struct CollateralPersonalProposal2 {
  static constexpr std::string_view kind = "family_collateral_proposed";
  CollateralPersonalTerms2 terms;
  FamilyPersonalExpression2 expression;
  std::optional<FamilyProposalRef2> previous_proposal_ref;
  std::optional<SourceRef> negotiated_reply_ref;
  Integer negotiation_round = 0;
  FieldOrder field_order;
};

struct CollateralPersonalReply2 {
  static constexpr std::string_view kind = "family_collateral_replied";
  FamilyProposalRef2 proposal_ref = FamilyCollateralProposalRef2{};
  FamilyPersonalReplyDecision2 decision = FamilyPersonalReplyDecision2::refused;
  FamilyPersonalExpression2 expression;
  std::optional<FamilyProposalRef2> counter_proposal_ref;
  FieldOrder field_order;
};

// 原 PersonalAccepted2 不再持祖系/条款或第三张同意；唯一来源是原 proposal。
struct CollateralPersonalAccepted2 {
  static constexpr std::string_view kind = "family_collateral_recognized";
  FamilyProposalRef2 proposal_ref = FamilyCollateralProposalRef2{};
  FamilyPersonalExpression2 expression;
  FieldOrder field_order;
};

// 仅本三类原 Common 地址。分配 ID、真实 header 和 command late-bind 归 writer。
SourceRef collateral_personal_proposal_source(const StableId &);
SourceRef collateral_personal_reply_source(const StableId &);
SourceRef collateral_personal_acceptance_source(const StableId &);
Status validate_collateral_personal_source_address(const SourceRef &);

// 下列是局部持值/跨字段关系，不替代已知源、成年、表达能力或历史 route 验证。
Status validate_collateral_personal_proposal_relations(
    const CollateralPersonalProposal2 &);
Status
validate_collateral_personal_reply_relations(const CollateralPersonalReply2 &);
Status validate_collateral_personal_acceptance_relations(
    const CollateralPersonalAccepted2 &);

Status visit_collateral_personal_proposal(CodecVisitor &,
                                          CollateralPersonalProposal2 &);
Status visit_collateral_personal_reply(CodecVisitor &,
                                       CollateralPersonalReply2 &);
Status visit_collateral_personal_acceptance(CodecVisitor &,
                                            CollateralPersonalAccepted2 &);
Status
emit_collateral_personal_proposal_fields(ProjectionWriter &,
                                         const CollateralPersonalProposal2 &);
Status emit_collateral_personal_reply_fields(ProjectionWriter &,
                                             const CollateralPersonalReply2 &);
Status
emit_collateral_personal_acceptance_fields(ProjectionWriter &,
                                           const CollateralPersonalAccepted2 &);
Outcome<BoundaryValue>
encode_collateral_personal_proposal(const CollateralPersonalProposal2 &);
Outcome<BoundaryValue>
encode_collateral_personal_reply(const CollateralPersonalReply2 &);
Outcome<BoundaryValue>
encode_collateral_personal_acceptance(const CollateralPersonalAccepted2 &);
Outcome<CollateralPersonalProposal2>
decode_collateral_personal_proposal(const BoundaryValue &);
Outcome<CollateralPersonalReply2>
decode_collateral_personal_reply(const BoundaryValue &);
Outcome<CollateralPersonalAccepted2>
decode_collateral_personal_acceptance(const BoundaryValue &);
} // namespace hanmo::resident
