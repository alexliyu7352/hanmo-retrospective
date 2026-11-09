// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "family_communication_read.hpp"
namespace hanmo::resident {
namespace {
// 失败保原来源地址；不会借相同 id 的另一 domain/kind 作为本次表达。
template <class T>
Outcome<T> reject(const char *code, const SourceRef &source,
                  ErrorKind kind = ErrorKind::internal) {
  return {false, {}, common_failure(code, source.id, kind).error};
}
bool same_source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
// 此层只核借用视图确实对应原 Common 头与固定公开角色，不复制 Family
// 的资格规则。
Outcome<FamilyCommunicationContent1>
project(const CommonStore &common, const SourceRef &source,
        const CollateralPersonalOriginalReadView &original,
        const FamilyCommunicationContentAudience1 &audience) {
  const auto *header = common.state().facts.find(source.id);
  if (!header || original.source_header != header || header->id != source.id ||
      header->domain != source.domain || header->kind != source.kind ||
      !original.proposal || !original.expression)
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationOriginalSourceMismatch", source);
  const auto *proposal_header =
      common.state().facts.find(original.proposal_source.id);
  if (!proposal_header ||
      original.proposal_source.domain != "family_collateral_legacy" ||
      original.proposal_source.kind != "family_collateral_proposed" ||
      proposal_header->id != original.proposal_source.id ||
      proposal_header->domain != original.proposal_source.domain ||
      proposal_header->kind != original.proposal_source.kind)
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationOriginalProposalMismatch", source);
  const auto &terms = original.proposal->terms;
  const auto &expression = *original.expression;
  const StableId *sender = nullptr, *receiver = nullptr;
  FamilyCommunicationContentKind kind;
  std::optional<FamilyCommunicationDecision> decision;
  switch (original.kind) {
  case CollateralPersonalExpressionKind2::proposed:
    if (source.kind != "family_collateral_proposed" || original.decision ||
        !same_source(source, original.proposal_source) ||
        original.expression != &original.proposal->expression)
      return reject<FamilyCommunicationContent1>(
          "FamilyCommunicationProposalSourceMismatch", source);
    sender = &terms.second_actor_id;
    receiver = &terms.first_actor_id;
    kind = FamilyCommunicationContentKind::initial_proposal;
    break;
  case CollateralPersonalExpressionKind2::replied:
    if (source.kind != "family_collateral_replied" || !original.decision)
      return reject<FamilyCommunicationContent1>(
          "FamilyCommunicationReplySourceMismatch", source);
    if (*original.decision == FamilyPersonalReplyDecision2::refused) {
      sender = &terms.first_actor_id;
      receiver = &terms.second_actor_id;
      kind = FamilyCommunicationContentKind::refused;
      decision = FamilyCommunicationDecision::refused;
    } else if (*original.decision == FamilyPersonalReplyDecision2::withdrawn) {
      sender = &terms.second_actor_id;
      receiver = &terms.first_actor_id;
      kind = FamilyCommunicationContentKind::withdrawn;
      decision = FamilyCommunicationDecision::withdrawn;
    } else {
      return reject<FamilyCommunicationContent1>(
          "FamilyCommunicationCollateralReplyUnsupported", source,
          ErrorKind::unsupported_capability);
    }
    break;
  case CollateralPersonalExpressionKind2::recognized:
    if (source.kind != "family_collateral_recognized" ||
        (original.decision &&
         *original.decision != FamilyPersonalReplyDecision2::accepted))
      return reject<FamilyCommunicationContent1>(
          "FamilyCommunicationAcceptanceSourceMismatch", source);
    sender = &terms.first_actor_id;
    receiver = &terms.second_actor_id;
    kind = FamilyCommunicationContentKind::accepted;
    decision = FamilyCommunicationDecision::accepted;
    break;
  default:
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationExpressionKindUnsupported", source,
        ErrorKind::unsupported_capability);
  }
  if (!sender || !receiver || sender->empty() || receiver->empty() ||
      *sender == *receiver || expression.actor_id != *sender ||
      expression.source_schema != 2 || terms.terms_version != 1)
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationOriginalParticipantsMismatch", source);
  // 作者自知只给真实 A 本人；最终收件人仍须外层真实 KnowledgeReceipt 才能获知。
  if (const auto *target =
          std::get_if<FamilyCommunicationFinalReceiver>(&audience)) {
    if (target->actor_id != *receiver)
      return reject<FamilyCommunicationContent1>(
          "FamilyCommunicationAudienceDenied", source, ErrorKind::rule);
  } else if (const auto *author =
                 std::get_if<FamilyCommunicationExpressionAuthor>(&audience)) {
    if (author->actor_id != *sender)
      return reject<FamilyCommunicationContent1>(
          "FamilyCommunicationAudienceDenied", source, ErrorKind::rule);
  } else {
    // 没有本次原已接受 MC1 服务来源，不能拿表达允许 carrier 当实际承运许可。
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationAcceptedCarrierOriginalRequired", source,
        ErrorKind::unsupported_capability);
  }
  FamilyCommunicationContent1 out;
  out.source_ref = source;
  out.content_kind = kind;
  out.sender_actor_id = *sender;
  out.receiver_actor_id = *receiver;
  out.proposal_ref = FamilyCollateralProposalRef2{original.proposal_source.id};
  out.proposal_source_ref = original.proposal_source;
  out.terms_version = terms.terms_version;
  out.terms_view =
      FamilyCollateralCommunicationTerms1{terms.terms_version,
                                          terms.first_actor_id,
                                          terms.second_actor_id,
                                          {original.proposal_source, 2}};
  out.decision = decision;
  out.expressed_at = expression.expressed_at;
  // v1e §5/§7 的完整固定 shape，Optional 即使为空也保原字段；
  // 此值是本投影的顺序，不能复用 A 私有表达或 proposal 的字段顺序。
  out.field_order = {"source_ref",
                     "source_schema",
                     "content_kind",
                     "sender_actor_id",
                     "receiver_actor_id",
                     "proposal_ref",
                     "proposal_source_ref",
                     "terms_version",
                     "terms_view",
                     "decision",
                     "previous_proposal_ref",
                     "negotiated_reply_ref",
                     "expressed_at",
                     "field_order"};
  // 旁支原文不允许协商，两个来源 Optional 严格保持无反提议；不复制私有
  // ancestry。
  return {true, std::move(out), {}};
}
} // namespace
// 先核活票与同世界 Family owner，再借一次真实已发布 A；无原源不回落 source1。
Outcome<FamilyCommunicationContent1> read_family_communication_content_v1(
    const FamilyCommunicationContentReadContext &context,
    const SourceRef &source,
    const FamilyCommunicationContentAudience1 &audience,
    const ReadStamp &stamp) {
  if (stamp.mode() != ReadMode::committed_current &&
      stamp.mode() != ReadMode::phase_current)
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationCurrentReadStampRequired", source,
        ErrorKind::unsupported_capability);
  auto current = context.authority.validate(
      stamp, context.common.state().identity, stamp.mode(), context.common,
      context.versions);
  if (!current.ok)
    return {false, {}, current.error};
  if (source.domain != "family_collateral_legacy" ||
      (source.kind != "family_collateral_proposed" &&
       source.kind != "family_collateral_replied" &&
       source.kind != "family_collateral_recognized"))
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationSourceNotSelected", source,
        ErrorKind::unsupported_capability);
  if (!context.collateral || !context.collateral->bound_to(context.common))
    return reject<FamilyCommunicationContent1>(
        "FamilyCommunicationCollateralOriginalRequired", source,
        ErrorKind::unsupported_capability);
  auto original = context.collateral->read_current_expression(source);
  if (!original.ok)
    return {false, {}, original.error};
  return project(context.common, source, original.value, audience);
}
} // namespace hanmo::resident
