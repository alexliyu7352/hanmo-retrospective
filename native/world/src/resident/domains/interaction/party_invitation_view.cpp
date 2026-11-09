// 本人当前邀请及真实队伍的有限表现；公开按钮不代替领域报价或本人选择。
#include "party_invitation_view.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/views/common_presentation.hpp"
namespace hanmo::resident {
Outcome<PartyParticipationView> query_party_participation(const CommonStore& common,const InteractionState& interaction,const InteractionCatalog& catalog,const PartyInvitationCatalog& invitations,const InteractionAccess& access,const StableId& actor){
    if(actor!=common.state().metadata.player_actor_id)return {false,{},common_failure("PartyViewObserverMismatch").error};
    const auto* person=common.actor(actor);if(!person||!catalog.common)return {false,{},common_failure("NoWorld").error};PartyParticipationView result{common.state().identity,actor,{}};CommonPresentation presentation(common.state(),*catalog.common);
    if(const auto* following=std::get_if<FollowingLocation>(&person->location)){
        auto leader=party_following_leader(common,actor);if(!leader.ok)return {false,{},leader.error};const auto* party=common.state().parties.find(following->party_id);const auto* terms=party->terms.find(actor);
        auto name=presentation.display_name(leader.value);if(!name.ok)return {false,{},name.error};auto leave=preview_leave_party(common,access,actor);
        result.rows.push_back({"membership",{},party->id,leader.value,std::move(name.value),terms->role_id,terms->cost_per_tick,leave.ok,leave.ok?StableId{}:leave.error.original.code});
        for(const auto& current:interaction.interactions)if(current.actor_id==actor){if(result.rows.back().conversation_open)return {false,{},common_failure("AmbiguousAuthoredNodeSource").error};result.rows.back().conversation_open=true;}
        return {true,std::move(result),{}};
    }
    const InteractionSession* visible=nullptr;for(const auto& current:interaction.interactions)if(current.actor_id==actor){if(visible)return {false,{},common_failure("AmbiguousAuthoredNodeSource").error};visible=&current;}
    if(!visible||(visible->phase!="say"&&visible->phase!="choice"&&visible->phase!="action"))return {true,std::move(result),{}};
    const PartyInvitationDefinition* invitation=nullptr;
    for(const auto& row:invitations.invitations)if(row.invitee_actor_id==actor&&row.leader_actor_id==visible->context.provider_actor_id&&row.dialogue_id==visible->dialogue_id&&row.node_id==visible->node_id&&row.role_id==visible->context.role_id){if(invitation)return {false,{},common_failure("InvalidPartyInvitationDefinition").error};invitation=&row;}
    if(!invitation)return {true,std::move(result),{}};
    // 当前真实节点证明失败就保留原错误；不以空页吞损坏，也不从冷作者表补一份邀请。
    auto proof=preview_authored_node_source(common,interaction,catalog,access,actor,invitation->leader_actor_id);if(!proof.ok)return {false,{},proof.error};auto quote=preview_party_invitation(common,interaction,catalog,invitations,access,actor,invitation->id);
    const auto* role=catalog.common->roles.find(invitation->role_id);if(!role)return {false,{},common_failure("InvalidPartyInvitationRole").error};auto name=presentation.display_name(invitation->leader_actor_id);if(!name.ok)return {false,{},name.error};
    result.rows.push_back({"invitation",invitation->id,{},invitation->leader_actor_id,std::move(name.value),invitation->role_id,role->companion_cost_per_tick,quote.ok,quote.ok?StableId{}:quote.error.original.code});return {true,std::move(result),{}};
}
BoundaryValue encode_party_participation(const PartyParticipationView& view){
    using V=BoundaryValue;StableId hex;const char* digits="0123456789abcdef";for(const auto byte:view.source_identity.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}V identity=boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{view.source_identity.execution_epoch}},{"revision",V{view.source_identity.revision}},{"tick",V{view.source_identity.tick}}});V::Array rows;
    for(const auto& row:view.rows){const bool invitation=row.kind=="invitation";V::Dict params;if(invitation)params.emplace_back("invitation_id",V{row.invitation_id});V action=boundary::obj({{"text",V{invitation?StableId{"接受随行邀请"}:StableId{"离开这支队伍"}}},{"command",V{invitation?StableId{"party_accept"}:StableId{"party_leave"}}},{"params",V{std::move(params)}},{"available",V{row.available}},{"code",V{row.code}}});V::Array actions{std::move(action)};if(!invitation){actions.push_back(boundary::obj({{"text",V{StableId{"随队等候半日"}}},{"command",V{StableId{"wait"}}},{"params",boundary::obj({})},{"available",V{true}}}));if(row.conversation_open)actions.push_back(boundary::obj({{"text",V{StableId{"结束这次交谈"}}},{"command",V{StableId{"end_interaction"}}},{"params",boundary::obj({})},{"available",V{true}}}));}V::Dict item{{"kind",V{row.kind}},{"leader_actor_id",V{row.leader_actor_id}},{"display_name",V{row.leader_name}},{"role_id",V{row.role_id}},{"cost_per_tick",V{row.cost_per_tick}},{"text",V{invitation?StableId{"对方已当面提出随行邀请，由你决定是否接受。"}:StableId{"你正随这位领队同行；离队须等城内停驻且双方没有行动。"}}},{"terms_text",V{"同行行旅每半日附加 "+std::to_string(row.cost_per_tick)+" 钱，由领队照原行旅约定支付；接受时不支付。"}},{"actions",V{std::move(actions)}}};if(invitation)item.emplace_back("invitation_id",V{row.invitation_id});else item.emplace_back("party_id",V{row.party_id});rows.push_back(V{std::move(item)});}
    return boundary::obj({{"ok",V{true}},{"source_identity",std::move(identity)},{"observer_actor_id",V{view.observer_actor_id}},{"rows",V{std::move(rows)}}});
}
}
