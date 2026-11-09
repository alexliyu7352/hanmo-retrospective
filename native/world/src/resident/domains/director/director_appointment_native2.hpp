// The complete World owner holds the two original Time2 context owners first.
// This object borrows them and owns only the original proposal read adapters.
#pragma once
#include "director_appointment_wake2.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_time2.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct WorldBundle;
class NativeDirectorAppointmentAttendanceRead2 final:public CheckpointAllocatedObject {
 const WorldBundle& world_;
 std::optional<VisitAppointmentAttendanceRead> written_;
 PersonalAppointmentAttendanceRead personal_;
 NativeDirectorAppointmentAttendanceRead2(const WorldBundle&,const PersonalAppointmentTime2Context&,const VisitAppointmentMeetingTime2Context*);
public:
 static Outcome<std::unique_ptr<NativeDirectorAppointmentAttendanceRead2>> create(const WorldBundle&,const PersonalAppointmentTime2Context&,const VisitAppointmentMeetingTime2Context* written=nullptr);
 NativeDirectorAppointmentAttendanceRead2(const NativeDirectorAppointmentAttendanceRead2&)=delete;
 NativeDirectorAppointmentAttendanceRead2&operator=(const NativeDirectorAppointmentAttendanceRead2&)=delete;
 const PersonalAppointmentAttendanceRead& read()const noexcept{return personal_;}
 // Explicit full cold gate, after real command/result binding. All original
 // wake enumeration/validation also invoke this same source validator.
 Status validate_original_sources()const;
};
}
