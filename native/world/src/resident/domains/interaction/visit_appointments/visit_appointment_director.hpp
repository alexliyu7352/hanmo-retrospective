// 模板13的原认领会话内有限续行；持久状态仍只有原Director和真实三源，不另造计划。
#pragma once
#include "visit_appointment_authored.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_director.hpp"
namespace hanmo::resident {
struct VisitAppointmentNpcRead{const VisitAppointmentCatalog& catalog;const VisitAppointmentState& state;const VisitAppointmentIntentCatalog& intents;const PersonalAppointmentCatalog& appointments;const PersonalAppointmentState& personal;const VisitReplyCatalog* ordinary=nullptr;const VisitAppointmentMeetingState* meetings=nullptr;const VisitAppointmentLifeContext* life=nullptr;};
struct VisitAppointmentNpcStep{DecisionProposal proposal;StableId command_kind;std::variant<InteractionCommand,AuthoredVisitAppointment> command;};
Outcome<bool> visit_appointment_reception_pending(const VisitReceptionRead&,const VisitAppointmentNpcRead&,const StableId& actor,const StableId& reception_intent);
// 外层只把这里已预览的具名command装回原CommandPayload，再走原NPC保存点与正式writer。
Outcome<std::optional<VisitAppointmentNpcStep>> propose_received_visit_appointment(const VisitReceptionRead&,const VisitAppointmentNpcRead&,const DirectorState&,const KnownSubjectView&,const StableId& reception_intent,const DecisionProposal&);
}
