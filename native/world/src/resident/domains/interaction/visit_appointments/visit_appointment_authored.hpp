#pragma once
#include "visit_appointment_intents.hpp"
namespace hanmo::resident {
class CurrentMomentRead;class NativeActorSocialOccupancyRead;
struct VisitAppointmentAuthorshipRead{const VisitAppointmentIntentCatalog& intents;const VisitReceptionCatalog& reception;const VisitReceptionState& claims;const InteractionState& interactions;const CurrentMomentRead* actual_clock=nullptr;const NativeActorSocialOccupancyRead* actual_occupancy=nullptr;};
struct AuthoredVisitAppointment{StableId intent_id,message_fact_id,received_fact_id,claim_fact_id;};
struct AuthoredVisitAppointmentQuote{AuthoredVisitAppointment command;VisitAppointmentQuote invitation;};
inline constexpr const char* authored_visit_appointment_command="offer_visit_appointment_authored";
Outcome<AuthoredVisitAppointmentQuote> preview_authored_visit_appointment(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionCatalog&,const InteractionAccess&,const VisitAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentAuthorshipRead&,const StableId&,const AuthoredVisitAppointment&,const VisitReplyCatalog* ordinary=nullptr,const VisitAppointmentMeetingState* meetings=nullptr,const VisitAppointmentLifeContext* life=nullptr);
Outcome<StableId> apply_authored_visit_appointment_in_candidate(CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionCatalog&,const InteractionAccess&,const VisitAppointmentCatalog&,VisitAppointmentState&,const VisitAppointmentAuthorshipRead&,const StableId&,const AuthoredVisitAppointment&,const AuthoredVisitAppointmentQuote&,const VisitReplyCatalog* ordinary=nullptr,const VisitAppointmentMeetingState* meetings=nullptr,const VisitAppointmentLifeContext* life=nullptr);
Status validate_visit_appointment_authored_sources(const CommonStore&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const InteractionCatalog&,const VisitAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentAuthorshipRead*,const VisitReplyCatalog* ordinary=nullptr);
}
