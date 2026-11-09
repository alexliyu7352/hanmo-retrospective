// Fixed S3 §4.2: the original invitation owns only its participation result.
#pragma once
#include "visit_appointment.hpp"
#include "visit_appointment_meeting.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/family/family_continuation_people4.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct VisitAppointmentLifeContext {
 const CommonStore& common;const VisitAppointmentState& state;
 const VisitAppointmentCatalog& catalog;const VisitMessageState& messages;
 const VisitMessageCatalog& message_catalog;const PersonalAppointmentCatalog& appointments;
 const InteractionCatalog& definitions;const VisitAppointmentMeetingState& meetings;
 const FamilyDeathPolicy& life_policy;const FamilyDeathState& deaths;
 const FamilyLifeCauseSources4& causes;const FamilyContinuationPeopleRead4& people;
 const CurrentMomentRead& clock;const VisitReplyCatalog* ordinary=nullptr;
};
// An already-started meeting must first adopt its single original terminal in
// the same death candidate. This leaf then references that result, never ends
// the Action again or manufactures a cancellation.
Status apply_visit_appointment_death_in_candidate(CommonStore&,VisitAppointmentState&,const VisitAppointmentLifeContext&,const DeathRef&);
Outcome<const VisitAppointmentLifeSource*> read_visit_appointment_life_source(const VisitAppointmentLifeContext&,const StableId&);
Status validate_visit_appointment_life_sources(const VisitAppointmentLifeContext&);
Status validate_visit_appointment_life_command(const VisitAppointmentLifeContext&,const StableId& actor,const VisitAppointmentCommand&);
Outcome<Integer> read_visit_appointment_calendar_now(const VisitAppointmentLifeContext&);
Outcome<VisitAppointmentState> make_initial_visit_appointment_state(const VisitAppointmentCatalog&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const InteractionCatalog&,const VisitReplyCatalog* ordinary=nullptr);
}
