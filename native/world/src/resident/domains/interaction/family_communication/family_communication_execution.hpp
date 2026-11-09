// S3 原窄卡 §4.2—4.4 的 B/D 值；只存本次真实执行，不拥有第二份 Family 或
// Knowledge 状态。
#pragma once
#include "family_communication_refs.hpp"
#include "resident/domains/family/family_expression_route.hpp"
#include "resident/domains/growth/life_actual_command_binding.hpp"
#include "resident/domains/interaction/interaction_context.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/model/commitment_ref.hpp"
namespace hanmo::resident {
// 原真实 say/choice 的最小历史快照，不允许后台捏造 session。
struct FamilyPostAuthoredSession1 {
  StableId session_id, session_actor_id, dialogue_id, node_id;
  InteractionContext context;
};
// 这里只保存 S1 已规范解析的真实 SettlementLocation
// 分支，不能以同城代替同场所。
struct FamilyPostNativePlace1 {
  StableId place_id;
  Location actor_location, listener_location;
  FixedLockedRuleInput contact_rule_ref;
};
using FamilyPostContact1 =
    std::variant<FamilyPostAuthoredSession1, FamilyPostNativePlace1>;
struct FamilyDirectExpression1 {
  FamilyPostContact1 contact;
};
// 本地已选分支是原 wire tag0。未接原 MC1 接受/运输来源之前，不提供 tag1 壳。
using FamilyCommunicationAcceptance1 = std::variant<FamilyDirectExpression1>;
struct FamilyCommunicationNoFee1 {
  FixedLockedRuleInput channel_no_fee_rule_ref;
};
// 当前只选原真实 NoFee；未选实际付款 owner 的正费用分支不得默认为免费。
using FamilyCommunicationCharge1 = std::variant<FamilyCommunicationNoFee1>;
struct FamilyCommunicationPostResult1 {
  StableId post_fact_id;
  SourceRef expression_source_ref;
  Integer expression_source_schema = 2;
  StableId actor_id;
  LifeActualCommandBinding post_command_binding;
  FamilyExpressionRoute post_route_basis;
  FixedLockedRuleInput channel_rule_ref;
  TermsRevisionRef channel_terms_ref;
  FamilyCommunicationAcceptance1 channel_acceptance;
  std::optional<SourceRef> expense_authority_source_ref;
  FamilyCommunicationCharge1 charge_source;
  std::optional<StableId> original_public_message_ref;
  WorldMoment posted_at;
};
// D 仍原完整字段；本地 Direct 模块明确禁止非空 D，不因型存在宣称远程可用。
struct FamilyCommunicationDeliveryResult1 {
  StableId delivery_fact_id;
  FamilyPostRef1 post_fact_ref;
  StableId carrier_actor_id;
  LifeActualCommandBinding delivery_command_binding;
  FamilyExpressionRoute delivery_route_basis;
  StableId original_public_message_ref;
  SourceRef completed_transport_source_ref;
  Integer completed_transport_source_schema = 0;
  FamilyPostContact1 contact;
  WorldMoment delivered_at;
};
} // namespace hanmo::resident
