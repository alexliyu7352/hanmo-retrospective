#include "resident/kernel/catalog_section_observation.hpp"
// 只接受具名原会面政策和有限提前量；旧目录缺省完全不激活NPC赴约。
#include "personal_appointment_attendance.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 原BoundaryValue直接调用同样拒绝重复字段，不能让作者与Native解出不同政策。
void unique(const BoundaryValue& value,const StableId& path){std::set<StableId> keys;for(const auto& row:boundary::object(value,path))if(!keys.insert(row.first).second)boundary::fail("DuplicateAppointmentAttendanceField",path);}
}
Outcome<bool> decode_personal_appointment_attendance_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;const auto* dynamic=scenario.find("dynamic_world");const auto* director=dynamic?dynamic->find("director"):nullptr;const auto* source=director?director->find("personal_appointment_attendance"):nullptr;if(!source)return {true,false,{}};
    auto parsed=decode_personal_appointment_attendance_catalog(*source);if(!parsed.ok)return {false,{},parsed.error};auto selected=decode_personal_appointment_profile_version(scenario,capabilities);if(!selected.ok)return {false,{},selected.error};if(!selected.value||!dynamic||get_string(*dynamic,"profile","dynamic_world")!="local_society_v1")unsupported("AppointmentAttendanceProfileRequired","personal_appointment_attendance");
    for(const auto* cap:{"personal_appointment_attendance_v1","npc_society","native_dynamic_world"})if(std::find(capabilities.begin(),capabilities.end(),cap)==capabilities.end())unsupported("AppointmentAttendanceCapabilityRequired",cap);return {true,true,{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Outcome<PersonalAppointmentAttendanceCatalog> decode_personal_appointment_attendance_catalog(const BoundaryValue& value){try{
    using namespace boundary;unique(value,"personal_appointment_attendance");fields(value,{"version","policies"},{},"personal_appointment_attendance");PersonalAppointmentAttendanceCatalog result;result.version=get_int(value,"version","personal_appointment_attendance");if(result.version!=1)unsupported("UnsupportedAppointmentAttendanceVersion","personal_appointment_attendance");
    const auto& policies=array(field(value,"policies","personal_appointment_attendance"),"policies");if(policies.empty()||policies.size()>4096)fail("InvalidAppointmentAttendancePolicies","policies");std::set<StableId> ids;
    for(const auto& row:policies){unique(row,"policy");fields(row,{"appointment_policy_id","depart_before_ticks"},{},"policy");PersonalAppointmentAttendancePolicy p{get_string(row,"appointment_policy_id","policy"),get_int(row,"depart_before_ticks","policy")};if(p.appointment_policy_id.empty()||p.appointment_policy_id.size()>256||p.depart_before_ticks<0||p.depart_before_ticks>720||!ids.insert(p.appointment_policy_id).second)fail("InvalidAppointmentAttendancePolicy","policy");result.policies.push_back(std::move(p));}
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_personal_appointment_attendance_catalog(const PersonalAppointmentAttendanceCatalog& attendance,const PersonalAppointmentCatalog& appointments){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 48, "PersonalAppointmentAttendanceCatalog", "validate_personal_appointment_attendance_catalog", &attendance);

    if(attendance.version!=1||(appointments.version!=1&&appointments.version!=2)||attendance.policies.empty()||attendance.policies.size()>4096)return common_failure("InvalidAppointmentAttendanceCatalog");std::set<StableId> ids;
    for(const auto& p:attendance.policies)if(p.appointment_policy_id.empty()||p.depart_before_ticks<0||p.depart_before_ticks>720||!ids.insert(p.appointment_policy_id).second||!personal_appointment_policy(appointments,p.appointment_policy_id))return common_failure("InvalidAppointmentAttendancePolicy",p.appointment_policy_id);return common_success();
}
}
