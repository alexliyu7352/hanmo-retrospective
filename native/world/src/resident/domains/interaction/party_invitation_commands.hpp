// 本人随行两命令保严格具名参数及原生报价，不提供任意成员/领队setter。
#pragma once
#include "party_invitation.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct PartyAcceptInvitation {StableId invitation_id;};
struct PartyLeaveOwnParty {};
using PartyParticipationCommand=std::variant<PartyAcceptInvitation,PartyLeaveOwnParty>;
// 原节点proof或本人原Party报价仅属当前执行点；不入存档。
struct PartyParticipationQuote {WorldIdentity source_identity;StableId actor_id;PartyParticipationCommand command;std::variant<PartyInvitationQuote,PartyLeaveQuote> detail;};
// 两个kind的公共注册必须同时受显式邀请profile守卫。
bool party_participation_command_kind(const StableId&);
StableId party_participation_command_kind(const PartyParticipationCommand&);
Outcome<PartyParticipationCommand> decode_party_participation_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_party_participation_command(const PartyParticipationCommand&);
BoundaryValue encode_party_participation_quote(const PartyParticipationQuote&);
// 只接受当前玩家明确选择；作者邀请本身不会令Director替其同意。
Outcome<PartyParticipationQuote> preview_party_participation_command(const CommonStore&,const InteractionState&,const InteractionCatalog&,const PartyInvitationCatalog*,const InteractionAccess&,const StableId& actor_id,const PartyParticipationCommand&);
// 调用者持原Common候选；所有失败须让外层恢复完整候选，成功只动原Party与本人位置。
Outcome<StableId> apply_party_participation_command(CommonStore&,const InteractionState&,const InteractionCatalog&,const PartyInvitationCatalog*,const InteractionAccess&,const StableId& actor_id,const PartyParticipationCommand&,const PartyParticipationQuote&);
}
