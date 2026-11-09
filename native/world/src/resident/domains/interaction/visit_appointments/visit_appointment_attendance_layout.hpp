// 书面赴约只有目录marker，没有持久段；标题/完整冷入口共用这项纯布局判定。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
inline Status validate_visit_appointment_attendance_layout(
    const std::vector<std::pair<StableId, StableId>> &modules,
    bool dynamic_enabled, bool has_state_section = false) {
  if (has_state_section)
    return common_failure("VisitAppointmentAttendanceHasNoStateSection");
  Integer selected = 0;
  for (const auto &row : modules)
    if (row.first == "visit_appointment_attendance") {
      if (row.second != "1")
        return common_failure("UnsupportedVisitAppointmentAttendanceVersion");
      if (++selected != 1)
        return common_failure("DuplicateVisitAppointmentAttendanceModule");
    }
  if (selected == 0)
    return common_success();
  if (!dynamic_enabled)
    return common_failure("VisitAppointmentAttendanceProfileRequired");
  // 原精确赴约已接受两份同版 /2 来源；个人出席目录 marker 仍只准 /1。
  StableId appointment_version;
  for (const auto *dependency :
       {"visit_appointment_invitations", "visit_appointment_meetings",
        "personal_appointment_attendance"}) {
    Integer count = 0;
    for (const auto &row : modules)
      if (row.first == dependency) {
        const bool personal_attendance =
            row.first == "personal_appointment_attendance";
        if ((row.second != "1" && (personal_attendance || row.second != "2")) ||
            ++count != 1 ||
            (!personal_attendance && !appointment_version.empty() &&
             appointment_version != row.second))
          return common_failure("VisitAppointmentAttendanceDependencyMismatch",
                                dependency);
        if (!personal_attendance)
          appointment_version = row.second;
      }
    if (count != 1)
      return common_failure("VisitAppointmentAttendanceDependencyRequired",
                            dependency);
  }
  return common_success();
}
} // namespace hanmo::resident
