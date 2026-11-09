// 新日期承诺的明确NPC意向；仍只沿原已领帖的真实接待会话，不把目录当一次自动承诺。
#pragma once
#include "visit_appointment.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_claims.hpp"
namespace hanmo::resident {
inline constexpr const char* visit_appointment_intent_module="visit_appointment_intents";
struct VisitAppointmentIntent{StableId id,reception_intent_id,invitation_policy_id;std::vector<VisitReceptionStep> route;};
struct VisitAppointmentIntentCatalog{Integer version=1;std::vector<VisitAppointmentIntent> intents;};
const VisitAppointmentIntent* visit_appointment_intent(const VisitAppointmentIntentCatalog&,const StableId&);
const VisitAppointmentIntent* visit_appointment_reception_intent(const VisitAppointmentIntentCatalog&,const StableId&);
Status validate_visit_appointment_intents(const VisitAppointmentIntentCatalog&,const VisitAppointmentCatalog&,const VisitReceptionCatalog&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const InteractionCatalog&,const VisitReplyCatalog* ordinary=nullptr);
Outcome<VisitAppointmentIntentCatalog> decode_visit_appointment_intents(const BoundaryValue&,const VisitAppointmentCatalog&,const VisitReceptionCatalog&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const InteractionCatalog&,const VisitReplyCatalog* ordinary=nullptr);
BoundaryValue encode_visit_appointment_intents(const VisitAppointmentIntentCatalog&);
}
