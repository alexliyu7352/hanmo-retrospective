// 隔离候选：原 S3 §4.1 四根完整 /2，绝不在旧三表 /1 上追加尾字段。
#pragma once
#include "family_communication_execution.hpp"
#include "family_message_business_types.hpp"
#include "resident/domains/social/social_knowledge_key.hpp"
namespace hanmo::resident {
class CommonStore;
struct ReadStamp;
struct FamilyCommunicationContentReadContext;
// I §3.4 的完整十四字段。当前具体内容型只登记 Family 的有限原源；
// 其余业务的旧 /1 原 bytes/reader 保留，不凭本型名宣称混合适配已齐。
struct FamilyMessageBusinessLink2 {
  FamilyMessageContentKind2 content_kind =
      FamilyMessageContentKind2::collateral_proposed;
  std::optional<SourceRef> source_fact_ref;
  std::optional<CommitmentRef> commitment_ref;
  std::optional<TermsRevisionRef> terms_revision;
  SubjectRef sender;
  RegisteredAddress addressed_to;
  FamilyMessageDisclosureScope2 disclosure_scope;
  MessageChannelKind channel = MessageChannelKind::face_to_face;
  std::optional<MessageCustodianRef2> custodian_ref;
  std::optional<StableId> physical_document_lot_id;
  Integer created_tick = 0, earliest_delivery_tick = 0;
  std::optional<Integer> expires_tick;
  std::optional<CommunicationReplyLinkRef2> reply_to_ref;
};
// entry 仍原规定次序：首业务键，随后展开上述十四字段，再四组原关联。
// business_link 只是 C++ 分组，不在 wire 另加这一层 object。
// knowledge refs 只引用 S0 已有 receiver/report 唯一键，不保存 learned 状态。
struct FamilyCommunicationLinkEntry2 {
  SourceRef original_business_fact_ref;
  FamilyMessageBusinessLink2 business_link;
  std::optional<StableId> original_public_message_ref;
  std::vector<SocialKnowledgeKey> original_knowledge_receipt_refs;
  std::vector<SourceRef> delivery_route_source_refs, dossier_receipt_refs;
};
// 三个表的键只用原 Common 全局唯一事实 ID：links 用 A，post/delivery 用 B/D。
// 入口与冷验仍反核完整 domain/kind/schema，不能只因同 ID 就承认另一类来源。
struct FamilyCommunicationLinksState2 {
  Integer version = 2;
  OrderedTable<FamilyCommunicationLinkEntry2> links;
  OrderedTable<FamilyCommunicationPostResult1> family_post_results;
  OrderedTable<FamilyCommunicationDeliveryResult1> family_delivery_results;
};
// 当前选型只旁支、Direct、NoFee。此函数仅局部形状/跨表指针及原头次序，
// 不替原 A、B 的身体/路由/实际命令/Knowledge validator，不得据此发布或装档。
Status
validate_family_direct_links_shape2(const CommonStore &,
                                    const FamilyCommunicationLinksState2 &);
// 单源 read 只验证本 B 对应的唯一 entry/A，供历史 A 因果读，禁止反调全局遍历。
Status validate_family_direct_link_projection2(
    const FamilyCommunicationContentReadContext &,
    const FamilyCommunicationLinksState2 &, const SourceRef &actual_post,
    const ReadStamp &);
// 真实 A 原历史 reader 再反核固定双方/条款/披露引用；不调用今天 first-send
// gate，不替 B 命令、接触能力或 Knowledge 来源的完整 validator。
Status validate_family_direct_link_projections2(
    const FamilyCommunicationContentReadContext &,
    const FamilyCommunicationLinksState2 &, const ReadStamp &);
} // namespace hanmo::resident
