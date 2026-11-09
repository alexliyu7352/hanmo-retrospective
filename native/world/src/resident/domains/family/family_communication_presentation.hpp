// LIFE v1f §6：Content2 的两个有限公开深值；不是礼议或婚议生产许可。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "resident/domains/growth/challenge_result_ref.hpp"
namespace hanmo::resident {
enum class FamilyEtiquettePublicCompletion {
  automatic,
  manual,
  exited_to_automatic
};
struct FamilyEtiquetteArgumentRef1 {
  StableId node_id, option_id;
  Integer definition_version = 0;
  FieldOrder field_order;
};
struct FamilyEtiquettePresentation1 {
  ChallengeResultRef result_ref;
  ChallengeDefinitionRef definition_ref;
  StableId performer_actor_id, recipient_actor_id;
  SourceRef previous_proposal_source_ref, counter_proposal_source_ref;
  FamilyEtiquettePublicCompletion completion =
      FamilyEtiquettePublicCompletion::automatic;
  std::vector<FamilyEtiquetteArgumentRef1> confirmed_argument_refs;
  FieldOrder field_order;
};
struct MarriageRequestPresentationView1 {
  SourceRef proposal_source_ref;
  Integer source_schema = 2, presentation_schema = 1;
  // 原锁行 ID，不能替换为 SourceRef，也不是私人身体能力 token。
  StableId request_binding_ref;
  StableId requester_actor_id, receiver_actor_id;
  WorldMoment formed_at;
  FieldOrder field_order;
};
struct BoundaryValue;
class CodecVisitor;
class ProjectionWriter;
// 唯一字段表的三出口仅证形状；真实 result/定义/SHA/表达反读仍由原 owner 做。
Status visit_family_etiquette_presentation(CodecVisitor &,
                                           FamilyEtiquettePresentation1 &);
Status visit_marriage_request_presentation(CodecVisitor &,
                                           MarriageRequestPresentationView1 &);
Status
emit_family_etiquette_presentation_fields(ProjectionWriter &,
                                          const FamilyEtiquettePresentation1 &);
Status emit_marriage_request_presentation_fields(
    ProjectionWriter &, const MarriageRequestPresentationView1 &);
Outcome<BoundaryValue>
encode_family_etiquette_presentation(const FamilyEtiquettePresentation1 &);
Outcome<BoundaryValue>
encode_marriage_request_presentation(const MarriageRequestPresentationView1 &);
Outcome<FamilyEtiquettePresentation1>
decode_family_etiquette_presentation(const BoundaryValue &);
Outcome<MarriageRequestPresentationView1>
decode_marriage_request_presentation(const BoundaryValue &);
} // namespace hanmo::resident
