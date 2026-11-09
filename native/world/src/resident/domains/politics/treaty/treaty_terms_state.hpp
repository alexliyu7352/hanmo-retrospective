// M-TREATY固定八根的唯一物理拥有体；履约、义务、知情与权源仍属原owner。
#pragma once
#include "treaty_mandate_sources.hpp"
#include "treaty_recognition_sources.hpp"
#include "treaty_release_sources.hpp"
#include "treaty_resource_body2.hpp"
namespace hanmo::resident {
struct PoliticalTreatyTermsState {
  Integer schema_version = 1;
  OrderedTable<TreatyOfferSource> offers;
  OrderedTable<TreatyCounterofferSource> counteroffers;
  OrderedTable<TreatyMandateSource> mandates;
  OrderedTable<TreatyPresentationSource> presentations;
  OrderedTable<TreatyReplyRecord> replies;
  OrderedTable<TreatySignatureSource> signatures;
  OrderedTable<TreatyActivationSource> activations;
  OrderedTable<TreatyTerminationSource> terminations;
  // 显式 /2 的唯一内部拥有体；/1 必须为空，原八根物理字节不变。
  std::optional<PoliticalTreatyResourceBody2> resource_body;
};
struct PoliticalTreatyTermsReadContext {
  const PoliticalTreatyTermsState &state;
  const TreatyRevisionContext &revisions;
  const TreatyMandateContext &mandates;
  const TreatyRecognitionContext &recognition;
  const TreatySignatureContext &signatures;
  const TreatyActivationContext &activations;
  const TreatyTerminationContext &terminations;
  const TreatyReleaseContext &releases;
};
// 显式同一candidate地址核后逐原owner历史门；缺任何真实处理者不能以空表冒准入。
Status
validate_political_treaty_terms_state(const PoliticalTreatyTermsReadContext &);
Status visit_political_treaty_terms(CodecVisitor &,
                                    PoliticalTreatyTermsState &);
Status
emit_political_treaty_terms_projection(ProjectionWriter &, std::string_view,
                                       const PoliticalTreatyTermsState &);
Outcome<BoundaryValue>
encode_political_treaty_terms(const PoliticalTreatyTermsState &);
// 仅还原私有值；World发布前仍必经上面的真实跨源全验证，无旧外交迁移或默认补源。
Outcome<PoliticalTreatyTermsState>
decode_political_treaty_terms(const BoundaryValue &);
// 中央 initialize/registry 只传有限原schema，不能把缺选型当 /1 或 /2。
Outcome<PoliticalTreatyTermsState>
initialize_political_treaty_terms_state(Integer selected_schema);
} // namespace hanmo::resident
