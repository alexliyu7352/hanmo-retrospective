// 原Director只在显式作者策略且真实当面会话中尝试发令，不替本人受领使命。
#pragma once
#include "diplomacy_delegation.hpp"
namespace hanmo::resident {
Outcome<std::optional<DiplomaticDelegateCommand>> propose_diplomatic_delegation_npc(
    const DiplomaticPresentRead&, const CommonCatalog&, const ActivityRead&, const StableId& actor,
    const StableId& delegation_policy_id);
}
