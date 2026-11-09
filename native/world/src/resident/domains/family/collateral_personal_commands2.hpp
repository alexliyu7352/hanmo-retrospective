// 原旁支三种公开命令的唯一纯DTO入口；不依赖World、Body、Core或source reader。
#pragma once
#include "collateral_family_origin_values.hpp"
#include "family_communication_options2.hpp"
#include "family_proposal_refs2.hpp"
#include "resident/domains/social/knowledge_source_ref.hpp"
namespace hanmo::resident {
struct BoundaryValue;
// 请求中的祖系只是一份待反验引用值，原read必须逐边重证；请求没有自由expression。
struct ProposeCollateralRecognition2 {
  StableId first_actor_id;
  FamilyTermsTemplateRef definition_ref;
  CollateralFamilyOrigin ancestry_source_ref;
  KnowledgeSourceRef known_target_ref;
  FamilyPersonalCommunicationOptions2 communication_options;
};
// 唯一原public初提payload；五个字段均必需，unknown/duplicate字段严格拒。
Outcome<ProposeCollateralRecognition2>
decode_propose_collateral_recognition2(const BoundaryValue &);
Outcome<BoundaryValue>
encode_propose_collateral_recognition2(const ProposeCollateralRecognition2 &);
struct RecognizeCollateral2 {
  FamilyCollateralProposalRef2 proposal_ref;
  KnowledgeSourceRef received_source_ref;
  FamilyPersonalCommunicationOptions2 communication_options;
};
struct RespondCollateralRecognition2 {
  FamilyCollateralProposalRef2 proposal_ref;
  FamilyPersonalReplyDecision2 decision = FamilyPersonalReplyDecision2::refused;
  std::optional<KnowledgeSourceRef> received_source_ref;
  FamilyPersonalCommunicationOptions2 communication_options;
};
using CollateralPersonalTerminalCommand2 =
    std::variant<RecognizeCollateral2, RespondCollateralRecognition2>;
Outcome<RecognizeCollateral2>
decode_recognize_collateral2(const BoundaryValue &);
Outcome<RespondCollateralRecognition2>
decode_respond_collateral_recognition2(const BoundaryValue &);
Outcome<BoundaryValue>
encode_recognize_collateral2(const RecognizeCollateral2 &);
Outcome<BoundaryValue>
encode_respond_collateral_recognition2(const RespondCollateralRecognition2 &);
} // namespace hanmo::resident
