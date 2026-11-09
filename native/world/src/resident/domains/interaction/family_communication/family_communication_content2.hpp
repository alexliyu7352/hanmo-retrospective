// LIFE v1f §6 的独立双尾公开 DTO；绝不改变旧 Content1 字节。
#pragma once
#include "family_communication_history.hpp"
#include "resident/domains/family/family_communication_presentation.hpp"
namespace hanmo::resident {
struct FamilyCommunicationContent2 {
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
  std::optional<FamilyEtiquettePresentation1> etiquette_presentation;
  std::optional<MarriageRequestPresentationView1> marriage_request_presentation;
  FieldOrder field_order;
};
// 原第九 KQ 六字段顺序不变，只由完整新选择采用真实 Content2。
struct KnownFamilyExpression2 {
  // 非 wire 首成员：先保费、末销毁，shared KQ 结果离开 reader 后仍保真实同 job
  // 租约。
  CheckpointMemoryCharge copy_memory;
  KnownFamilyExpression2() = default;
  KnownFamilyExpression2(const KnownFamilyExpression2 &) = delete;
  KnownFamilyExpression2 &operator=(const KnownFamilyExpression2 &) = delete;
  KnownFamilyExpression2 &operator=(KnownFamilyExpression2 &&) = delete;
  // 原精确分数右值仍会复制且可能抛出。全部成员成功后才转移租约；
  // 若中途失败，源对象仍持原费直到其剩余字段真正销毁，不能default先搬charge。
  KnownFamilyExpression2(KnownFamilyExpression2 &&other)
      : report_id(std::move(other.report_id)),
        expression_source_ref(std::move(other.expression_source_ref)),
        expression_source_schema(other.expression_source_schema),
        content(std::move(other.content)), observed_at(other.observed_at),
        learned_at(other.learned_at) {
    copy_memory = std::move(other.copy_memory);
  }

  StableId report_id;
  SourceRef expression_source_ref;
  Integer expression_source_schema = 2;
  FamilyCommunicationContent2 content;
  WorldMoment observed_at, learned_at;
};
Status validate_family_content2_selection(
    const FamilyCommunicationContentReadContext &);
Status validate_family_content2_identity_selection(const CatalogIdentity &);
Status validate_family_content2_selection(const CommonStore &,
                                          const CatalogIdentity &);
Status validate_family_content2_catalog_binding(
    const FamilyCommunicationContentReadContext &,
    const CatalogIdentity &actual);
Outcome<FamilyCommunicationContent2> read_family_communication_content_v2(
    const FamilyCommunicationContentReadContext &, const SourceRef &,
    const FamilyCommunicationContentAudience1 &, const ReadStamp &);
Outcome<FamilyCommunicationContent2>
read_family_communication_content_before_fact_v2(
    const FamilyCommunicationHistoryReadContext &, const SourceRef &,
    const FamilyCommunicationContentAudience1 &, const SourceRef &,
    const ReadStamp &);
// 原同 job 拥有者必须活到返回值全部销毁；此口不创建另一预算或补记事后分配。
Outcome<FamilyCommunicationContent2> read_family_communication_content_owned_v2(
    const FamilyCommunicationContentReadContext &, const SourceRef &,
    const FamilyCommunicationContentAudience1 &, const ReadStamp &,
    CheckpointMemoryCharge &returned_memory);
Outcome<FamilyCommunicationContent2>
read_family_communication_content_before_fact_owned_v2(
    const FamilyCommunicationHistoryReadContext &, const SourceRef &,
    const FamilyCommunicationContentAudience1 &, const SourceRef &,
    const ReadStamp &, CheckpointMemoryCharge &returned_memory);
} // namespace hanmo::resident
