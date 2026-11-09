#include "director_appointment_native2.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
NativeDirectorAppointmentAttendanceRead2::NativeDirectorAppointmentAttendanceRead2(const WorldBundle&w,const PersonalAppointmentTime2Context&personal,const VisitAppointmentMeetingTime2Context*written)
 :world_(w),personal_{w.common,w.interaction,*w.catalog->interaction,*w.catalog->personal_appointments,*w.interaction.personal_appointments,nullptr,w.catalog->visit_appointment_attendance.get(),&personal}{
 if(written){written_.emplace(VisitAppointmentAttendanceRead{w.common,w.interaction,w.tasks,*w.catalog->visit_appointment_invitations,*w.interaction.visit_appointment_invitations,*w.interaction.visit_appointment_meetings,*w.catalog->personal_appointments,*w.catalog->director->personal_appointment_attendance,written});personal_.written_visits=&*written_;}
}
Outcome<std::unique_ptr<NativeDirectorAppointmentAttendanceRead2>>NativeDirectorAppointmentAttendanceRead2::create(const WorldBundle&w,const PersonalAppointmentTime2Context&personal,const VisitAppointmentMeetingTime2Context*written){
 if(!w.catalog||!w.catalog->director||!w.catalog->director->subjects2||!w.director.subject2||!w.catalog->director->personal_appointment_attendance||!w.catalog->interaction||!w.catalog->personal_appointments||!w.interaction.personal_appointments)return {false,{},common_failure("DirectorAppointmentSelectedOwnerRequired",{},ErrorKind::unsupported_capability).error};
 if(&personal.common!=&w.common||&personal.interaction!=&w.interaction||&personal.catalog!=w.catalog->personal_appointments.get()||&personal.state!=&*w.interaction.personal_appointments)return {false,{},common_failure("DirectorAppointmentCandidateMismatch",{},ErrorKind::internal).error};
 if(bool(w.catalog->visit_appointment_attendance)!=bool(written))return {false,{},common_failure("DirectorVisitAppointmentTime2OwnerRequired",{},ErrorKind::unsupported_capability).error};
 if(written&&(!w.catalog->visit_appointment_invitations||!w.interaction.visit_appointment_invitations||!w.interaction.visit_appointment_meetings||&written->common!=&w.common||&written->interaction!=&w.interaction||&written->catalog!=w.catalog->personal_appointments.get()||&written->invitations!=&*w.interaction.visit_appointment_invitations||&written->state!=&*w.interaction.visit_appointment_meetings))return {false,{},common_failure("DirectorVisitAppointmentCandidateMismatch",{},ErrorKind::internal).error};
 if(personal.catalog.version!=2||personal.state.version!=2||(written&&(written->invitations.version!=2||written->state.version!=2)))return {false,{},common_failure("DirectorAppointmentTime2SourcesRequired",{},ErrorKind::unsupported_capability).error};
 auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto at=personal.clock.read_current_moment();if(!at.ok)return {false,{},at.error};auto same=compare_world_moments(at.value,now.value);if(!same.ok)return {false,{},same.error};if(same.value)return {false,{},common_failure("DirectorAppointmentActualClockMismatch",{},ErrorKind::internal).error};
 if(written){auto other=written->clock.read_current_moment();if(!other.ok)return {false,{},other.error};same=compare_world_moments(other.value,now.value);if(!same.ok)return {false,{},same.error};if(same.value)return {false,{},common_failure("DirectorVisitAppointmentActualClockMismatch",{},ErrorKind::internal).error};}
 // Constructing the same-candidate borrower precedes the real result binder.
 // Full historical source validation belongs to the explicit cold/wake gate.
 return {true,std::unique_ptr<NativeDirectorAppointmentAttendanceRead2>(new NativeDirectorAppointmentAttendanceRead2(w,personal,written)),{}};
}
Status NativeDirectorAppointmentAttendanceRead2::validate_original_sources()const{
 auto now=world_.actual_phase_moment();if(!now.ok)return {false,{},now.error};
 if(!world_.catalog||!world_.catalog->director||!world_.catalog->director->personal_appointment_attendance)return common_failure("DirectorAppointmentSelectedOwnerRequired",{},ErrorKind::unsupported_capability);
 return validate_director_appointment_wake_sources2({personal_,*world_.catalog->director->personal_appointment_attendance,world_.director,*world_.catalog->director,now.value});
}
}
