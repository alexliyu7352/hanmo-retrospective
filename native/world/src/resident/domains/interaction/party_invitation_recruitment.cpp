// 本叶只判专用岗位方向，不替代原会面、成员、占用、关系或费用校验。
#include "party_invitation_recruitment.hpp"
namespace hanmo::resident {
Status validate_legacy_party_recruitment(const PartyInvitationCatalog* invitations,const StableId& role){
    if(invitations)for(const auto& source:invitations->invitations)if(source.role_id==role)return common_failure("PartyInvitationAcceptanceRequired");
    return common_success();
}
}
