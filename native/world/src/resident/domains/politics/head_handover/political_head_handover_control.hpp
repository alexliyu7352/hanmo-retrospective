// 生前让职只借原Family/军事占领纯史，不借家族换控授予原Office或跳过辖区控制。
#pragma once
#include "political_head_handover_contract.hpp"
#include "resident/domains/politics/succession/political_succession_control.hpp"
namespace hanmo::resident {
// 原完整Family/Born/Legacy控制史先验后核本次具名头，两种序列同侧才可认定历史本人。
Outcome<StableId> read_political_head_handover_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& boundary);
// 共享原只读结构，不反调完整政治/Shop账；占领后的新让职政策须另有真实连续性规则。
Status validate_political_head_handover_seat(const PoliticalSuccessionTerritoryContext&,const PoliticalSuccessionPolicy&,Integer tick,const StableId& boundary={});
}
