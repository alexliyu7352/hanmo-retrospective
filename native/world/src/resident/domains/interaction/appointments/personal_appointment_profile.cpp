// 作者、独立段与能力严格同启用；不会因目录里存在一段对白就自动替人物预约。
#include "personal_appointment.hpp"
#include <set>
namespace hanmo::resident {
Outcome<Integer> decode_personal_appointment_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){
    try{using namespace boundary;std::set<StableId> names;for(const auto& row:object(scenario,"scenario"))if(!names.insert(row.first).second)fail("DuplicatePersonalAppointmentField",row.first);
        const auto* profile=scenario.find("personal_appointment_profile");const auto* source=scenario.find("personal_appointments");
        if(!profile){if(source)unsupported("PersonalAppointmentProfileRequired","personal_appointments");return {true,0,{}};}
        const auto& selected=string(*profile,"personal_appointment_profile");const Integer version=selected=="witnessed_personal_meetings_v1"?1:selected=="witnessed_personal_meetings_time2_v2"?2:0;if(!version)unsupported("UnsupportedPersonalAppointmentProfile","personal_appointment_profile");if(!source)unsupported("PersonalAppointmentCatalogRequired","personal_appointments");
        if(get_int(*source,"version","personal_appointments")!=version)unsupported("PersonalAppointmentCatalogVersionMismatch","personal_appointments");
        for(const auto* cap:{version==1?"personal_appointments_v1":"personal_appointments_v2","dialogue","multi_actor","native_dynamic_world"})if(std::find(capabilities.begin(),capabilities.end(),cap)==capabilities.end())unsupported("PersonalAppointmentCapabilityRequired",cap);
        const auto* dynamic=scenario.find("dynamic_world");if(!dynamic||get_string(*dynamic,"profile","dynamic_world")!="local_society_v1")unsupported("PersonalAppointmentNativeProfileRequired","dynamic_world");return {true,version,{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Outcome<bool> decode_personal_appointment_profile(const BoundaryValue& source,const std::vector<StableId>& capabilities){auto r=decode_personal_appointment_profile_version(source,capabilities);if(!r.ok)return {false,{},r.error};return {true,r.value!=0,{}};}
Status validate_personal_appointment_layout(const CommonStore& common,const std::vector<std::pair<StableId,StableId>>& modules,const PersonalAppointmentCatalog* catalog,const PersonalAppointmentState* state){
    Integer count=0;for(const auto& p:modules)if(p.first==personal_appointment_module){++count;if(!catalog||p.second!=std::to_string(catalog->version))return common_failure("UnsupportedPersonalAppointmentVersion");}
    if(count!=(catalog?1:0)||bool(catalog)!=bool(state)||(catalog&&catalog->version!=1&&catalog->version!=2)||(state&&state->version!=catalog->version))return common_failure("PersonalAppointmentProfileMismatch");if(state)return common_success();
    for(const auto& row:common.state().facts.rows()){const auto& h=row.second;if(h.domain==personal_appointment_module||h.kind==personal_appointment_accepted_kind||h.kind==personal_appointment_cancelled_kind||h.kind==personal_appointment_started_kind||h.kind==personal_appointment_finished_kind)return common_failure("PersonalAppointmentProfileRequired",row.first);}
    for(const auto& row:common.state().actions.rows())if(row.second.kind==personal_appointment_action_kind)return common_failure("PersonalAppointmentProfileRequired",row.first);return common_success();
}
}
