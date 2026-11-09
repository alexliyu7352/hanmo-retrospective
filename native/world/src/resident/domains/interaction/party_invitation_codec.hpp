// 作者邀请仅编译具名双方和当前对白节点；不接收任意脚本、费用或运行成员状态。
#pragma once
#include "party_invitation.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
// 本块是独立作者合同，旧Common Party的持久字节完全不变。
inline constexpr Integer party_invitation_catalog_version=1;
Outcome<PartyInvitationCatalog> decode_party_invitation_catalog(const BoundaryValue&,const InteractionCatalog&);
BoundaryValue encode_party_invitation_catalog(const PartyInvitationCatalog&);
}
