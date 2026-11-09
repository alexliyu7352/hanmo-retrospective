// 原Family唯一控制链决定实际回复者当时是否本人；不由今日metadata或缓存存在倒推。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <functional>
namespace hanmo::resident {
class CommonStore;struct FamilyState;struct FamilyLegacyState;
using GovernanceInvitationPlayerHistoryRead=std::function<Outcome<StableId>(Integer tick,const StableId& actual_reply_fact)>;
// 调用前完整Family/Birth/Legacy/弟子全史已验；只接本模块真实关闭头的精确事实前界。
Outcome<StableId> read_governance_invitation_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_reply_fact);
}
