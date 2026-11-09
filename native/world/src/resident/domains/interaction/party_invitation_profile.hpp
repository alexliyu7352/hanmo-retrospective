// 邀请profile只保护明确作者目录和新增命令历史；Party本体继续使用原Common字节。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct PartyInvitationCatalog;class CommonStore;
// 所选情景独立启用，组合包的能力并集不会替旧情景开启邀请。
Outcome<bool> decode_party_invitation_profile(const BoundaryValue&,const std::vector<StableId>& required_capabilities);
// 无新持久域；启用时必须有唯一version1 marker及已编译只读目录。
Status validate_party_invitation_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& module_versions,const PartyInvitationCatalog*);
// 旧profile保原Party，但不能暗含新party_accept/party_leave命令历史。
Status validate_party_invitation_history(const CommonStore&,bool enabled);
}
