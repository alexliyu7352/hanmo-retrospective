// 同一World栈中借用原承诺，目录标记与计划来源先验不增加第二份会面状态。
#pragma once
#include "personal_appointment_attendance.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 初始化第一次Director解码前核真实完整目录，不用空catalog绕过显式模板准入。
Status validate_personal_appointment_attendance_author_source(const BoundaryValue& scenario,const BoundaryValue& data);
Status validate_personal_appointment_attendance_world(const WorldBundle&);
Status wake_personal_appointment_attendance_world(WorldBundle&);
}
