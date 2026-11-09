// One compiled provider, three original owners. No copied registry or world.
#pragma once
#include "appointments/personal_appointment_native.hpp"
#include "contact_inquiries/contact_inquiry_native.hpp"
#include "visit_appointments/visit_appointment_native.hpp"
namespace hanmo::resident {
struct InteractionExactWorldFactories {
 PersonalAppointmentExactWorldFactory personal;
 ContactInquiryExactWorldFactory inquiry;
 VisitAppointmentExactWorldFactory visit;
};
InteractionExactWorldFactories native_interaction_exact_world_factories();
ExactPhaseRegistration make_interaction_exact_phase_registration(InteractionExactWorldFactories);
Status apply_interaction_appointment_death_in_candidate(WorldBundle&,const DeathRef&,InteractionExactWorldFactories,ExactPhaseWriteContext* actual_writes=nullptr);
}
