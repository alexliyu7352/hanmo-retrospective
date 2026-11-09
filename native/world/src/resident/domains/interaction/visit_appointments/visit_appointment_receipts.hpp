// 三种本人零时业务都必须有本次真实Common结果；裁剪后不能拿更早邀请替换这次来源。
#pragma once
#include "visit_appointment_state.hpp"
#include "resident/domains/interaction/visit_messages/visit_authored_command.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Outcome<StableId> visit_appointment_new_player_source(const VisitAppointmentState* before,const VisitAppointmentState* after,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick);
Status bind_visit_appointment_player_result(CommonStore&,VisitAppointmentState&,const StableId& source_fact_id,const StableId& actor,Integer request_sequence,const CommitResult&);
Status validate_visit_appointment_cached_results(const CommonStore&,const VisitAppointmentState*,const VisitAuthoredCommandRead* actual_commands=nullptr);
// Real before/after delta, before Common seal and adoption. Does not write a player binding.
Outcome<MomentOwnerSource> read_visit_appointment_director_command_effect(
 const CommonStore&,const VisitAppointmentState& before,const VisitAppointmentState& after,
 const StableId& actor,const ReceiptDetails&,const WorldMoment& actual_at);
Status validate_visit_appointment_director_actor_effect(const CommonStore&,const VisitAppointmentState&,
 const VisitAuthoredCommandRead&,const StableId& actor,const SourceRef&);
}
