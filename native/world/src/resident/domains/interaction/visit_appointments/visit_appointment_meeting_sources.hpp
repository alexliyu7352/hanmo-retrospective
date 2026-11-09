#pragma once
#include "visit_appointment_meeting.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyLegacyState;
// 完整原Family/Birth/Legacy后，只对本模块真实三类会面头读取原唯一控制链。
Outcome<StableId> read_visit_appointment_meeting_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_fact);
// 原结构、三源和全部人物来源先验；历史人物以真实创建/军事死亡头截面，不用今日alive反推。
Status validate_visit_appointment_meeting_sources(const CommonStore&,const CommonCatalog&,const VisitAppointmentMeetingState&,const FamilyDeathOriginRead&,const VisitAppointmentPlayerHistoryRead&);
}
