// 原事实前双方必须已经存在且在世；后来真实死亡保留旧答允，不用当前alive倒改历史。
#include "personal_appointment_presence.hpp"
namespace hanmo::resident {
Status validate_personal_appointment_presence(const CommonStore& common,const CommonCatalog& catalog,const PersonalAppointmentState& state,const FamilyDeathOriginRead& deaths,const DynamicStepInput* phase){
    auto contact=[&](const PersonalAppointmentContact& c,const StableId& fact,const StableId& kind)->Status{
        const auto* header=common.state().facts.find(fact);if(!header||header->id!=fact||header->kind!=kind||header->domain!=personal_appointment_module)return common_failure("PersonalAppointmentPresenceBoundaryRequired",fact);
        for(const auto* actor:{&c.actor_id,&c.provider_actor_id}){auto presence=read_actor_presence_before_fact(common,catalog,deaths,*actor,c.tick,fact,phase);if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return common_failure("PersonalAppointmentParticipantNotAliveAtSource",fact);}
        return common_success();
    };
    for(const auto& row:state.accepted.rows()){auto valid=contact(row.second.contact,row.first,personal_appointment_accepted_kind);if(!valid.ok)return valid;}
    for(const auto& row:state.cancelled.rows()){auto valid=contact(row.second.contact,row.first,personal_appointment_cancelled_kind);if(!valid.ok)return valid;}
    for(const auto& row:state.started.rows()){auto valid=contact(row.second.contact,row.first,personal_appointment_started_kind);if(!valid.ok)return valid;}
    // 完成成功还须双方在真实完成头前在世；失败可来自当时不可用/会话改变，不伪造第二份死因。
    for(const auto& row:state.finished.rows())if(row.second.success){const auto& done=row.second;const auto* start=state.started.find(done.start_fact_id);if(!start)return common_failure("PersonalAppointmentPresenceStartRequired",row.first);auto c=start->contact;c.tick=done.tick;auto valid=contact(c,row.first,personal_appointment_finished_kind);if(!valid.ok)return valid;}
    return common_success();
}
}
