// 本人预约页只显示实际当面答允的约定和眼前新的声明；不同人物不共享私人约期列表。
#pragma once
#include "personal_appointment.hpp"
namespace hanmo::resident {
struct PersonalAppointmentViewAccess {
    const CommonCatalog& common_catalog;
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const InteractionAccess& interaction_access;
    const PersonalAppointmentCatalog& catalog;
    const PersonalAppointmentState& state;
    const PersonalAppointmentTime2Context* exact=nullptr;
};
Outcome<BoundaryValue> query_personal_appointments(const CommonStore&,const PersonalAppointmentViewAccess&,Integer cursor=0);
}
