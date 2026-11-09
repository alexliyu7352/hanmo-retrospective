// 生活安排只继续本人已实际启动的原会面Action；目的地或预约本身不能冒充已开工来源。
#pragma once
#include "personal_appointment_state.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct PersonalAppointmentTime2Context;
// 原contact供Plan核同一say会话例外，不授对方的新同意或透露今日远方去向。
struct PersonalAppointmentActionSource {
    WorldIdentity source_identity;
    StableId actor_id,appointment_id,action_id,started_fact_id,finished_fact_id,status,reason;
    PersonalAppointmentContact contact;
    Integer start_tick=0,remaining_ticks=0,finished_tick=-1;
    bool success=false;
    std::optional<WorldMoment> actual_started_at,actual_finished_at;
};
// 调用前须同World完整预约冷验；未知/他人动作返回空，本域缺原永久来源返回错误。
Outcome<std::optional<PersonalAppointmentActionSource>> query_personal_appointment_action(
    const CommonStore&,const PersonalAppointmentState&,const StableId& actor_id,const StableId& action_id,const PersonalAppointmentTime2Context* exact=nullptr);
// 仅匹配本次真实Advance的精确ActionCompletion，后来其他预约不能替代原终态。
Outcome<std::optional<PersonalAppointmentActionSource>> query_personal_appointment_completion(
    const CommonStore&,const PersonalAppointmentState&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer completed_tick,const StableId& actual_result_id={},const PersonalAppointmentTime2Context* exact=nullptr);
}
