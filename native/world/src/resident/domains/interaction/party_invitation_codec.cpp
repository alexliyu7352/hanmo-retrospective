// 严格作者块只允许声明一份当前节点邀请；实际意愿与费用仍沿已验证的定义和原岗位条款。
#include "party_invitation_codec.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
Outcome<PartyInvitationCatalog> decode_party_invitation_catalog(const BoundaryValue& input,const InteractionCatalog& interaction){
    try{
        boundary::fields(input,{"version","invitations"},{},"party_invitations");
        if(boundary::get_int(input,"version","party_invitations")!=party_invitation_catalog_version)return {false,{},common_failure("UnsupportedPartyInvitationVersion").error};
        PartyInvitationCatalog result;
        for(const auto& row:boundary::array(boundary::field(input,"invitations","party_invitations"),"party_invitations/invitations")){
            const StableId path="party_invitations/invitations";boundary::fields(row,{"id","leader_actor_id","invitee_actor_id","dialogue_id","node_id","role_id"},{},path);
            result.invitations.push_back({boundary::get_string(row,"id",path),boundary::get_string(row,"leader_actor_id",path),boundary::get_string(row,"invitee_actor_id",path),boundary::get_string(row,"dialogue_id",path),boundary::get_string(row,"node_id",path),boundary::get_string(row,"role_id",path)});
        }
        auto valid=validate_party_invitation_catalog(result,interaction);if(!valid.ok)return {false,{},valid.error};return {true,std::move(result),{}};
    }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
BoundaryValue encode_party_invitation_catalog(const PartyInvitationCatalog& catalog){
    using V=BoundaryValue;V::Array rows;
    for(const auto& row:catalog.invitations)rows.push_back(boundary::obj({
        {"id",V{row.id}},{"leader_actor_id",V{row.leader_actor_id}},
        {"invitee_actor_id",V{row.invitee_actor_id}},{"dialogue_id",V{row.dialogue_id}},
        {"node_id",V{row.node_id}},{"role_id",V{row.role_id}}}));
    return boundary::obj({{"version",V{party_invitation_catalog_version}},{"invitations",V{std::move(rows)}}});
}
}
