// 只有明确启用的专用邀请岗位禁止旧反向招募；普通岗位与旧profile保原准入。
#pragma once
#include "party_invitation.hpp"
namespace hanmo::resident {
// 空目录表示未启用profile；已编译的专用role只能经当前节点的本人party_accept受领。
Status validate_legacy_party_recruitment(const PartyInvitationCatalog* enabled_invitations,const StableId& role_id);
}
