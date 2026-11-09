// 随行公共组合不分配第二事实/费用账；命令、当前身份与领域原quote逐项一致。
#include "party_invitation_commands.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
namespace {
// 完整身份防读档、换局与半日后的旧同意继续生效。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
}
bool party_participation_command_kind(const StableId& kind){return kind=="party_accept"||kind=="party_leave";}
StableId party_participation_command_kind(const PartyParticipationCommand& c){return std::holds_alternative<PartyAcceptInvitation>(c)?"party_accept":"party_leave";}
Outcome<PartyParticipationCommand> decode_party_participation_command(const StableId& kind,const BoundaryValue& value){try{
    using namespace boundary;
    if(kind=="party_accept"){fields(value,{"invitation_id"},{},"party_accept");auto id=get_string(value,"invitation_id","party_accept");if(id.empty())fail("InvalidPartyInvitation","party_accept/invitation_id");return {true,PartyAcceptInvitation{std::move(id)},{}};}
    if(kind=="party_leave"){fields(value,{},{},"party_leave");return {true,PartyLeaveOwnParty{},{}};}
    return {false,{},common_failure("UnsupportedPartyParticipationCommand","kind",ErrorKind::unsupported_capability).error};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
BoundaryValue encode_party_participation_command(const PartyParticipationCommand& c){using V=BoundaryValue;if(const auto* accept=std::get_if<PartyAcceptInvitation>(&c))return boundary::obj({{"invitation_id",V{accept->invitation_id}}});return boundary::obj({});}
Outcome<PartyParticipationQuote> preview_party_participation_command(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const PartyInvitationCatalog* invitations,const InteractionAccess& access,const StableId& actor,const PartyParticipationCommand& command){
    if(!invitations)return {false,{},common_failure("PartyInvitationsUnavailable",{},ErrorKind::unsupported_capability).error};
    if(actor!=common.state().metadata.player_actor_id)return {false,{},common_failure("PartyParticipantNotPlayer").error};
    if(const auto* accept=std::get_if<PartyAcceptInvitation>(&command)){auto q=preview_party_invitation(common,state,catalog,*invitations,access,actor,accept->invitation_id);if(!q.ok)return {false,{},q.error};return {true,{common.state().identity,actor,command,std::move(q.value)},{}};}
    auto q=preview_leave_party(common,access,actor);if(!q.ok)return {false,{},q.error};return {true,{common.state().identity,actor,command,std::move(q.value)},{}};
}
Outcome<StableId> apply_party_participation_command(CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const PartyInvitationCatalog* invitations,const InteractionAccess& access,const StableId& actor,const PartyParticipationCommand& command,const PartyParticipationQuote& quote){
    if(!same_identity(common.state().identity,quote.source_identity)||quote.actor_id!=actor||party_participation_command_kind(command)!=party_participation_command_kind(quote.command)||!compare_boundary(encode_party_participation_command(command),encode_party_participation_command(quote.command)).exact())return {false,{},common_failure("StalePartyParticipationQuote").error};
    if(!invitations)return {false,{},common_failure("PartyInvitationsUnavailable",{},ErrorKind::unsupported_capability).error};
    if(actor!=common.state().metadata.player_actor_id)return {false,{},common_failure("PartyParticipantNotPlayer").error};
    if(const auto* accept=std::get_if<PartyAcceptInvitation>(&command)){const auto* q=std::get_if<PartyInvitationQuote>(&quote.detail);if(!q)return {false,{},common_failure("PartyInvitationQuoteMismatch").error};return accept_party_invitation_in_candidate(common,state,catalog,*invitations,access,actor,accept->invitation_id,*q);}
    const auto* q=std::get_if<PartyLeaveQuote>(&quote.detail);if(!q)return {false,{},common_failure("PartyInvitationQuoteMismatch").error};return leave_party_in_candidate(common,access,actor,*q);
}
BoundaryValue encode_party_participation_quote(const PartyParticipationQuote& quote){
    using V=BoundaryValue;V::Dict result{{"kind",V{party_participation_command_kind(quote.command)}},{"actor_id",V{quote.actor_id}},{"duration",V{Integer{0}}},{"params",encode_party_participation_command(quote.command)}};
    if(const auto* q=std::get_if<PartyInvitationQuote>(&quote.detail)){result.emplace_back("leader_actor_id",V{q->source.provider_actor_id});result.emplace_back("cost_per_tick",V{q->cost_per_tick});}
    else {const auto& leave=std::get<PartyLeaveQuote>(quote.detail);result.emplace_back("party_id",V{leave.party_id});result.emplace_back("leader_actor_id",V{leave.leader_actor_id});result.emplace_back("place_id",V{leave.place_id});}
    return V{std::move(result)};
}
}
