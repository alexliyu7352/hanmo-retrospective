// 待中央审的 /2 首次工程映射；不改变已发行 /1，也不启用新的消息渠道。
#pragma once
#include "family_communication_refs.hpp"
#include "resident/domains/interaction/communication_delivery_read.hpp"
namespace hanmo::resident {
// C++ 枚举仅限 S3 家庭窄卡 §2.1 的十五个原源。wire 仍是原 content_kind
// 字符串，不把本枚举序号或 FamilyCommunicationContent1 的展示类别写进去。
enum class FamilyMessageContentKind2 {
  marriage_proposed,
  marriage_replied,
  birth_proposed,
  birth_replied,
  birth_accepted,
  adult_adoption_proposed,
  adult_adoption_replied,
  adult_adoption_accepted,
  collateral_proposed,
  collateral_replied,
  collateral_recognized,
  apprenticeship_proposed,
  apprenticeship_replied,
  education_proposed,
  education_chosen
};
// 名称与地址仅给静态分派，不证明 A 已发布、可发送或观察者已知。
const char *
    family_message_content_kind_name2(FamilyMessageContentKind2) noexcept;
Outcome<FamilyMessageContentKind2>
read_family_message_content_kind2(std::string_view wire_name);
Outcome<FamilyMessageContentKind2>
family_message_content_kind_from_source2(const SourceRef &);

// disclosure_scope 的家庭分支只引用 A；其 DirectOnly/AcceptedCarrierDelivery
// 和公开条款由同根 Family owner 反读，不能在 link 另写一份可扩权的受众列表。
// 拟议新 /2 wire：tag=0，expression_ref={exact_source_ref,source_schema:2}。
// 原义并非“tag0 即可披露”：实际 B/服务/receiver 门仍分别核验。
struct FamilyExpressionDisclosureScope2 {
  FamilyExpressionRef2 expression_ref;
};
using FamilyMessageDisclosureScope2 =
    std::variant<FamilyExpressionDisclosureScope2>;

// 复用 S3 真实 Actor/可投递物件身份，绝不复用 S2 的同名看管权源。
// Direct 必空；其他已选渠道只有实际 MC1 保管来源能填写，carrier 不自动
// 成为 custodian，也不由本 ref 获得正文或领取权限。
using MessageCustodianRef2 = CommunicationCustodian;

// reply_to_ref 规范化为唯一原 link 的业务源键，不伪造一个 Common 事实头。
// 拟议新 /2 wire：{original_business_fact_ref:SourceRef}。
// Family 须以实际 causal receipt 反读此 entry 的非空原 message；当面为空，
// 自己 outbound 不能当来信。此值不重复保存 message 或 Knowledge 状态。
struct CommunicationReplyLinkRef2 {
  SourceRef original_business_fact_ref;
};
} // namespace hanmo::resident
