// 当面随行邀请只使用明确作者节点和本人同意；成员与费用仍唯一保存在原Common Party。
#pragma once
#include "dialogue_node_source.hpp"
namespace hanmo::resident {
// 作者明确谁邀请谁；节点本身仅证明当前来源，邀请意愿由这份具名定义赋予。
struct PartyInvitationDefinition {
    StableId id,leader_actor_id,invitee_actor_id,dialogue_id,node_id,role_id;
};
// 首版邀请只在当前会话可接受，不新增离场待办、承诺钟或默认NPC答应规则。
struct PartyInvitationCatalog { std::vector<PartyInvitationDefinition> invitations; };
// 临时报价包含完整执行身份；接受后保原Party与其条款，不把旧epoch保存成永久权限。
struct PartyInvitationQuote {
    AuthoredNodeSourceProof source;
    StableId invitation_id,party_id,role_id;
    Integer cost_per_tick=0;
};
// 本人离队只在城内停驻边界；不能借此拆开原行旅Action或改领队路线。
struct PartyLeaveQuote {
    WorldIdentity source_identity;
    StableId actor_id,party_id,leader_actor_id,settlement_id,place_id;
};
// 校验作者对象及唯一节点绑定，不因一个普通会面自动产生邀请。
Status validate_party_invitation_catalog(const PartyInvitationCatalog&,const InteractionCatalog&);
// 仅本人当前明确邀请可见；不向外人泄漏另一人的作者邀请是否存在。
Outcome<PartyInvitationQuote> preview_party_invitation(const CommonStore&,const InteractionState&,const InteractionCatalog&,const PartyInvitationCatalog&,const InteractionAccess&,const StableId& actor_id,const StableId& invitation_id);
// 调用方持同一候选/保存点；占用与来源重验后只写原Party及本人的Following位置。
Outcome<StableId> accept_party_invitation_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const PartyInvitationCatalog&,const InteractionAccess&,const StableId& actor_id,const StableId& invitation_id,const PartyInvitationQuote&);
// 纯成员来源供本人视图使用，原始Following引用必须与真成员表和直接领队相符。
Outcome<StableId> party_following_leader(const CommonStore&,const StableId& actor_id);
// 半日网关额外核本人可推进与原Action参与引用；领队自己的独立活动不冻结全世界。
Outcome<StableId> party_clock_leader(const CommonStore&,const StableId& actor_id);
// 报价与执行都核本人、领队及原当前Party；零时退出不产生关系或资金后果。
Outcome<PartyLeaveQuote> preview_leave_party(const CommonStore&,const InteractionAccess&,const StableId& actor_id);
Outcome<StableId> leave_party_in_candidate(CommonStore&,const InteractionAccess&,const StableId& actor_id,const PartyLeaveQuote&);
}
