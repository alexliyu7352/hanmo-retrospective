#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp"
// 赴约只改变原Director调度；没有预约侧账、额外预算或强制结束会面。
#include "personal_appointment_attendance_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
Status validate_personal_appointment_attendance_author_source(const BoundaryValue& scenario,const BoundaryValue& data){try{using namespace boundary;
    auto common=decode_common_catalog(data);if(!common.ok)return {false,{},common.error};auto interaction=decode_interaction_catalog(data,checkpoint_make_shared<const CommonCatalog>(std::move(common.value)));if(!interaction.ok)return {false,{},interaction.error};
    auto appointments=decode_personal_appointment_catalog(field(scenario,"personal_appointments","scenario"),interaction.value);if(!appointments.ok)return {false,{},appointments.error};const auto& dynamic=field(scenario,"dynamic_world","scenario");const auto& director=field(dynamic,"director","dynamic_world");auto attendance=decode_personal_appointment_attendance_catalog(field(director,"personal_appointment_attendance","director"));if(!attendance.ok)return {false,{},attendance.error};return validate_personal_appointment_attendance_catalog(attendance.value,appointments.value);
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_personal_appointment_attendance_world(const WorldBundle& w){
    const auto* catalog=w.catalog->director&&w.catalog->director->personal_appointment_attendance?w.catalog->director->personal_appointment_attendance.get():nullptr;Integer markers=0;for(const auto& marker:w.catalog->identity.module_versions)if(marker.first=="personal_appointment_attendance"){++markers;if(marker.second!="1")return common_failure("UnsupportedAppointmentAttendanceVersion");}
    if(markers!=(catalog?1:0)||catalog&&(!w.dynamic_enabled()||!w.catalog->personal_appointments||!w.interaction.personal_appointments))return common_failure("AppointmentAttendanceProfileMismatch");
    if(catalog){auto valid=validate_personal_appointment_attendance_catalog(*catalog,*w.catalog->personal_appointments);if(!valid.ok)return valid;}
    const PersonalAppointmentState empty;const auto& state=w.interaction.personal_appointments?*w.interaction.personal_appointments:empty;
    for(const auto* plans:{&w.director.plans,&w.director.archived_plans})for(const auto& row:plans->rows()){if(w.catalog->visit_appointment_attendance&&visit_appointment_attendance_goal(row.second.goal_ref))continue;auto valid=validate_personal_appointment_attendance_plan(w.common,state,catalog,row.second);if(!valid.ok)return valid;}
    return common_success();
}
Status wake_personal_appointment_attendance_world(WorldBundle& w){
    if(!w.catalog->director||!w.catalog->director->personal_appointment_attendance)return common_success();if(!w.dynamic_enabled()||!w.catalog->personal_appointments||!w.interaction.personal_appointments)return common_failure("AppointmentAttendanceProfileMismatch");
    PersonalAppointmentAttendanceRead read{w.common,w.interaction,*w.catalog->interaction,*w.catalog->personal_appointments,*w.interaction.personal_appointments};return wake_personal_appointment_attendance(read,*w.catalog->director->personal_appointment_attendance,*w.catalog->director,w.director);
}
}
