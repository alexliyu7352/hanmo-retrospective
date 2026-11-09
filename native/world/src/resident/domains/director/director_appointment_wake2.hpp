// Original attendance commitments only. Calendar admission gates are observed at
// the current boundary; that observation is never an invitation event Moment.
#pragma once
#include "director_native_wakes2.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_attendance.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance.hpp"
namespace hanmo::resident {
struct DirectorAppointmentWakeRead2 {
 const PersonalAppointmentAttendanceRead& original;
 const PersonalAppointmentAttendanceCatalog& catalog;
 const DirectorState& director;const DirectorCatalog& policies;
 const WorldMoment& actual;
};
Status validate_director_appointment_wake_sources2(const DirectorAppointmentWakeRead2&);
Status visit_director_appointment_wakes2(const DirectorAppointmentWakeRead2&,const DirectorOriginalWakeVisitor2&);
Status validate_director_appointment_wake2(const DirectorAppointmentWakeRead2&,const SubjectWake2&);
// Full original plan-source validation is kept for both live and archived plans.
Status validate_director_appointment_plan2(const DirectorAppointmentWakeRead2&,const FinitePlan&);
bool director_appointment_wake_domain2(const SourceRef&)noexcept;
}
