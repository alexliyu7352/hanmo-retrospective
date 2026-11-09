// 新目录/marker与历史校验均纯读，不给旧档补同意记录、成员或新字段。
#include "party_invitation_profile.hpp"
#include "party_invitation.hpp"
#include "resident/storage/common_codec.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
Outcome<bool> decode_party_invitation_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;object(scenario,"scenario");const auto* profile=scenario.find("party_invitation_profile");if(!profile){if(scenario.find("party_invitations"))unsupported("UnexpectedPartyInvitationCatalog","scenario/party_invitations");return {true,false,{}};}
    if(string(*profile,"scenario/party_invitation_profile")!="authored_node_v1")unsupported("UnsupportedPartyInvitationProfile","scenario/party_invitation_profile");
    // 同一原生世界、人物、对白和队伍已有能力组合；新准入仍由显式profile/marker严格区分。
    for(const auto* required:{"native_dynamic_world","multi_actor","dialogue","companions"})if(std::find(capabilities.begin(),capabilities.end(),required)==capabilities.end())unsupported("MissingPartyInvitationCapability",StableId{"required_capabilities/"}+required);
    const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)unsupported("PartyInvitationNativeProfileRequired","scenario/dynamic_world");
    const auto* native=dynamic->find("profile");if(!native||string(*native,"scenario/dynamic_world/profile")!="local_society_v1")unsupported("PartyInvitationNativeProfileRequired","scenario/dynamic_world/profile");
    const auto* block=scenario.find("party_invitations");if(!block)unsupported("MissingPartyInvitationCatalog","scenario/party_invitations");fields(*block,{"version","invitations"},{},"scenario/party_invitations");if(get_int(*block,"version","scenario/party_invitations")!=1)unsupported("UnsupportedPartyInvitationVersion","scenario/party_invitations/version");array(field(*block,"invitations","scenario/party_invitations"),"scenario/party_invitations/invitations");
    return {true,true,{}};
}catch(const boundary::DecodeFailure& failure){return {false,false,failure.error};}}
Status validate_party_invitation_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const PartyInvitationCatalog* catalog){
    const StableId* version=nullptr;for(const auto& pair:modules)if(pair.first=="party_invitations"){if(version)return common_failure("DuplicatePartyInvitationMarker");version=&pair.second;}
    if(!enabled){if(version||catalog)return common_failure("UnexpectedPartyInvitationProfile");return common_success();}
    if(!version)return common_failure("MissingPartyInvitationMarker");if(*version!="1")return common_failure("UnsupportedPartyInvitationVersion");
    if(!catalog)return common_failure("MissingPartyInvitationCatalog");return common_success();
}
Status validate_party_invitation_history(const CommonStore& common,bool enabled){
    if(enabled)return common_success();
    for(const auto& row:common.state().results.rows())if(row.second&&(row.second->kind=="party_accept"||row.second->kind=="party_leave"))return common_failure("PartyInvitationProfileRequired","results/"+row.first);
    for(const auto& row:common.state().history)if(row.kind=="party_accept"||row.kind=="party_leave")return common_failure("PartyInvitationProfileRequired","history/"+row.result_id);
    return common_success();
}
}
