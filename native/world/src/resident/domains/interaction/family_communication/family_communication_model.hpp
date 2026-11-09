// v1e §5.1 的原只读消息内容；此批只接旁支三个真实 source2，不新增持久世界段。
#pragma once
#include "family_communication_refs.hpp"
#include "resident/domains/family/collateral_personal.hpp"
namespace hanmo::resident {
struct FamilyCommunicationTermsRef2 {
  FamilyProposalRef2 proposal_ref = FamilyCollateralProposalRef2{};
  Integer terms_version = 1;
};
enum class FamilyCommunicationExpressionKind {
  initial_proposal,
  counter_proposal,
  accepted,
  refused,
  withdrawn
};
enum class FamilyCommunicationDecision { accepted, refused, withdrawn };
enum class FamilyCommunicationSendability { sendable, not_standalone_sendable };
// 只引用原不可变提议和真实答复；不泄露私有决定依据、控制来源或祖系。
struct FamilyProposalTermsDisclosure2 {
  FamilyCommunicationTermsRef2 terms_ref;
  std::optional<FamilyProposalRef2> previous_proposal_ref;
  std::optional<SourceRef> negotiated_reply_ref;
};
struct FamilyDecisionDisclosure2 {
  SourceRef decision_source_ref;
  FamilyCommunicationTermsRef2 terms_ref;
  FamilyCommunicationDecision decision = FamilyCommunicationDecision::refused;
};
// 本叶仅登记 ProposalTerms/Decision；EducationChoice 由其原 owner
// 接齐后才可选。
using FamilyCommunicationDisclosure2 =
    std::variant<FamilyProposalTermsDisclosure2, FamilyDecisionDisclosure2>;
struct FamilyCommunicationSourceView2 {
  SourceRef source_ref;
  Integer source_schema = 2;
  FamilyExpressionRef2 expression_ref;
  StableId sender_actor_id, receiver_actor_id;
  FamilyProposalRef2 proposal_ref = FamilyCollateralProposalRef2{};
  SourceRef proposal_source_ref;
  FamilyCommunicationTermsRef2 family_terms_ref;
  FamilyCommunicationExpressionKind expression_kind =
      FamilyCommunicationExpressionKind::initial_proposal;
  WorldMoment expressed_at;
  FamilyCommunicationSendability sendability =
      FamilyCommunicationSendability::sendable;
  FamilyCommunicationDisclosure2 disclosure;
  std::optional<SourceRef> causal_received_source_ref;
  std::optional<SourceRef> reply_to_communication_ref;
  FamilyCommunicationBudget communication_budget;
  FamilyCommunicationAudience communication_audience =
      FamilyCommunicationAudience::direct_only;
  FieldOrder field_order;
};
// wire tag 3 的原 Collateral 公开条款。ancestry_ref 不展开未披露的完整祖系。
struct FamilyCollateralAncestryRef1 {
  SourceRef proposal_source_ref;
  Integer source_schema = 2;
};
struct FamilyCollateralCommunicationTerms1 {
  static constexpr Integer wire_tag = 3;
  Integer terms_version = 1;
  StableId first_actor_id, second_actor_id;
  FamilyCollateralAncestryRef1 ancestry_ref;
};
// 只有已施工的真实条款 owner 能进入本联合；不以空壳冒其它五种已实现。
using FamilyCommunicationTermsView1 =
    std::variant<FamilyCollateralCommunicationTerms1>;
enum class FamilyCommunicationContentKind {
  initial_proposal,
  counter_proposal,
  accepted,
  refused,
  withdrawn,
  education_choice
};
struct FamilyCommunicationContent1 {
  SourceRef source_ref;
  Integer source_schema = 2;
  FamilyCommunicationContentKind content_kind =
      FamilyCommunicationContentKind::initial_proposal;
  StableId sender_actor_id, receiver_actor_id;
  FamilyProposalRef2 proposal_ref = FamilyCollateralProposalRef2{};
  SourceRef proposal_source_ref;
  Integer terms_version = 1;
  FamilyCommunicationTermsView1 terms_view;
  std::optional<FamilyCommunicationDecision> decision;
  std::optional<FamilyProposalRef2> previous_proposal_ref;
  std::optional<SourceRef> negotiated_reply_ref;
  WorldMoment expressed_at;
  FieldOrder field_order;
};
// S3 窄卡 §2.3 第九 KQ 叶的六字段；不是第二条收知记录。
struct KnownFamilyExpression1 {
  StableId report_id;
  SourceRef expression_source_ref;
  Integer expression_source_schema = 2;
  FamilyCommunicationContent1 content;
  WorldMoment observed_at, learned_at;
};
struct FamilyCommunicationFinalReceiver {
  StableId actor_id;
};
struct FamilyCommunicationAcceptedCarrier {
  StableId actor_id;
  SourceRef communication_service_source_ref;
};
struct FamilyCommunicationExpressionAuthor {
  StableId actor_id;
};
using FamilyCommunicationContentAudience1 =
    std::variant<FamilyCommunicationFinalReceiver,
                 FamilyCommunicationAcceptedCarrier,
                 FamilyCommunicationExpressionAuthor>;
} // namespace hanmo::resident
