#pragma once
#include "visit_appointment.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyLegacyState;
// 完整Family/Birth/Legacy2/弟子先验后，仅合原唯一控制链与这次实际三源头的截面。
Outcome<StableId> read_visit_appointment_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_fact);
}
