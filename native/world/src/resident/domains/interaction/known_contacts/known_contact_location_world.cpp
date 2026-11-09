// 同候选栈内持有死亡来源及原会面/文书只读引用；调用结束后不缓存World借用。
#include "known_contact_location_world.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_known_contact_location_world(const WorldBundle& w,Integer cursor){
    if(!w.catalog->common||!w.catalog->interaction||!w.catalog->interaction->scene)return {false,{},common_failure("KnownContactCatalogRequired").error};
    const auto* appointments=w.interaction.personal_appointments?&*w.interaction.personal_appointments:nullptr;auto valid=validate_personal_appointment_layout(w.common,w.catalog->identity.module_versions,w.catalog->personal_appointments.get(),appointments);if(!valid.ok)return {false,{},valid.error};
    const auto* messages=w.tasks.message_handoffs?&*w.tasks.message_handoffs:nullptr;if(w.catalog->task_message_handoffs_enabled!=(w.catalog->task_message_handoff_version!=0))return {false,{},common_failure("TaskMessageHandoffProfileMismatch").error};valid=validate_task_message_handoff_layout(w.common,w.catalog->task_message_handoffs_enabled,w.catalog->identity.module_versions,messages,w.catalog->task_message_handoffs_enabled?w.catalog->task_message_handoff_version:1);if(!valid.ok)return {false,{},valid.error};
    if(!w.dynamic_enabled()||(!w.catalog->personal_appointments&&!w.catalog->task_message_handoffs_enabled))return {false,{},common_failure("KnownContactSourceProfileRequired").error};
    auto death_origins=w.family_death_read();std::optional<AppointmentContactHistory> appointment_read;std::optional<MessageContactHistory> message_read;
    if(w.catalog->personal_appointments){valid=validate_personal_appointment_world(w);if(!valid.ok)return {false,{},valid.error};appointment_read.emplace(AppointmentContactHistory{w.interaction,*w.catalog->interaction,*w.catalog->personal_appointments,*appointments,death_origins});}
    if(w.catalog->task_message_handoffs_enabled){if(!w.catalog->tasks||!messages)return {false,{},common_failure("KnownContactMessageSourcesRequired").error};message_read.emplace(MessageContactHistory{w.tasks,*w.catalog->tasks,*messages});}
    KnownContactLocationAccess read{w.common,*w.catalog->common,appointment_read?&*appointment_read:nullptr,message_read?&*message_read:nullptr};return query_known_contact_locations(read,*w.catalog->interaction->scene,cursor);
}
}
