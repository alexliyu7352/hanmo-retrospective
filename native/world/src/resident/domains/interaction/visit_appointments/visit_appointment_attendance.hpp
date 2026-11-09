// 书面邀请赴约显式复用原模板15和提前量，只履行本人的原发邀承诺，不窥探另一端收取/接受。
#pragma once
#include "visit_appointment_access.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_attendance.hpp"
namespace hanmo::resident {
struct TaskState;struct VisitAppointmentMeetingTime2Context;
inline constexpr const char* visit_appointment_attendance_module="visit_appointment_attendance";
struct VisitAppointmentAttendanceCatalog {Integer version=1;std::vector<StableId> invitation_policy_ids;};
// 新目录只选择具名书面政策；提前量和原NPC模板资格仍来自既有当面赴约目录。
Outcome<VisitAppointmentAttendanceCatalog> decode_visit_appointment_attendance_catalog(const BoundaryValue&);
BoundaryValue encode_visit_appointment_attendance_catalog(const VisitAppointmentAttendanceCatalog&);
Status validate_visit_appointment_attendance_catalog(const VisitAppointmentAttendanceCatalog&,const VisitAppointmentCatalog&,const PersonalAppointmentAttendanceCatalog&,const PersonalAppointmentCatalog&);
Outcome<bool> decode_visit_appointment_attendance_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
// 同World完整三源/人物/控制/会面/结果/跨承诺已验后借用；没有新的持久状态或远方定位读。
struct VisitAppointmentAttendanceRead {
    const CommonStore& common;
    const InteractionState& interaction;
    const TaskState& tasks;
    const VisitAppointmentCatalog& invitations;
    const VisitAppointmentState& state;
    const VisitAppointmentMeetingState& meetings;
    const PersonalAppointmentCatalog& appointments;
    const PersonalAppointmentAttendanceCatalog& original_attendance;
    const VisitAppointmentMeetingTime2Context* exact=nullptr;
};
Status validate_visit_appointment_attendance_sources(const VisitAppointmentAttendanceRead&,const VisitAppointmentAttendanceCatalog&);
// 原Task优先。到场仅等候本人已承诺的窗口，不替收件人接受，也不建立双人Action或关会话。
Outcome<std::optional<DecisionProposal>> propose_visit_appointment_attendance(const VisitAppointmentAttendanceRead&,const VisitAppointmentAttendanceCatalog&,const TravelCatalog&,const KnownSubjectView&,const DecisionProposal&);
// 只调整原已登记模板15下一次复评，不移动公平游标，不提供额外同tick决定次数。
Status wake_visit_appointment_attendance(const VisitAppointmentAttendanceRead&,const VisitAppointmentAttendanceCatalog&,const DirectorCatalog&,DirectorState&);
// 与旧当面计划门按原goal的具名来源分派；新格式只认本人的真实offered，禁止拿collected/accepted替代。
bool visit_appointment_attendance_goal(const StableId&);
Status validate_visit_appointment_attendance_plan(const CommonStore&,const VisitAppointmentState&,const VisitAppointmentAttendanceCatalog*,const FinitePlan&);
}
