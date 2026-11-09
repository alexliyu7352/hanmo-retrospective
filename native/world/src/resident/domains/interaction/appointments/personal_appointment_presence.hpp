// 预约的本人存在/当时生死晚门；不授知情、成年或今日角色身份，不替代原预约及Family全史。
#pragma once
#include "personal_appointment_state.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident {
// 必须在同候选Military、Family、Creation/Birth及预约结构全验之后调用。
Status validate_personal_appointment_presence(const CommonStore&,const CommonCatalog&,const PersonalAppointmentState&,const FamilyDeathOriginRead&,const DynamicStepInput* phase=nullptr);
}
