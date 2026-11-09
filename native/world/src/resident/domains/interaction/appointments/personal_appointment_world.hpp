// 世界接缝只借同候选Common/Interaction；原保存点、时钟与统一Action释放由中央拥有。
#pragma once
#include "personal_appointment_boundary.hpp"
#include "personal_appointment_view.hpp"
#include "personal_appointment_time2.hpp"
#include "personal_appointment_progress.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct PersonalAppointmentWorldResult {bool action_started=false;StableId source_id;};
// commit_tail前取原前后候选新增源，真实result生成后再写各原事实尾。
Outcome<std::vector<StableId>> personal_appointment_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& kind,const ReceiptDetails&);
Status bind_personal_appointment_result_world(WorldBundle&,const std::vector<StableId>&,const StableId& actor,Integer request_sequence,const CommitResult&,const PersonalAppointmentTime2Context* exact=nullptr);
Status validate_personal_appointment_world(const WorldBundle&,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<PersonalAppointmentQuote> prepare_personal_appointment_world(const WorldBundle&,const StableId& actor,const PersonalAppointmentCommand&,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<PersonalAppointmentWorldResult> commit_personal_appointment_world(WorldBundle&,const StableId& actor,const PersonalAppointmentCommand&,const PersonalAppointmentQuote&,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<PersonalAppointmentCompletion> step_personal_appointment_world(WorldBundle&,const ActionCoreView&,const DynamicStepInput&);
Outcome<BoundaryValue> query_personal_appointment_world(const WorldBundle&,Integer cursor=0,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<std::optional<PersonalAppointmentActionSource>> query_personal_appointment_action_world(const WorldBundle&,const StableId& actor,const StableId& action,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<std::optional<PersonalAppointmentActionSource>> query_personal_appointment_completion_world(const WorldBundle&,const StableId& actor,const StableId& action,const ReceiptDetails&,Integer completed_tick,const StableId& actual_result_id,const PersonalAppointmentTime2Context* exact=nullptr);

}
