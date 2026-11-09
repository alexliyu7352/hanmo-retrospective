// 书面赴约只有原Director借用与目录标记；完整visit史先验后再验证派生计划，没有第二预约账。
#pragma once
#include "visit_appointment_attendance.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 值对象仅含本候选只读引用；调用者持有整个optional直到原proposer同步返回。
Outcome<std::optional<VisitAppointmentAttendanceRead>> visit_appointment_attendance_read_world(const WorldBundle&);
Status validate_visit_appointment_attendance_world(const WorldBundle&);
Status wake_visit_appointment_attendance_world(WorldBundle&);
}
