// 原本人只履行已经当面承诺的会面；独立作者启用不产生新预约、会话或隐蔽位置知识。
#pragma once
#include "personal_appointment.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
struct VisitAppointmentAttendanceRead;struct VisitAppointmentAttendanceCatalog;
inline constexpr const char* personal_appointment_attendance_template="personal_appointment_attendance";
// 中央已预留独立模板15；旧13拜帖和14接册保持原身份。
inline constexpr Integer personal_appointment_attendance_template_number=15;
// 提前量只决定开始复评赴约的时间，不延长原窗口、保证到场或预支路费。
struct PersonalAppointmentAttendancePolicy {StableId appointment_policy_id;Integer depart_before_ticks=1;};
struct PersonalAppointmentAttendanceCatalog {Integer version=1;std::vector<PersonalAppointmentAttendancePolicy> policies;};
// 显式作者能力与原预约profile配对；只改Director目录，不扩旧持久布局。
Outcome<bool> decode_personal_appointment_attendance_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<PersonalAppointmentAttendanceCatalog> decode_personal_appointment_attendance_catalog(const BoundaryValue&);
Status validate_personal_appointment_attendance_catalog(const PersonalAppointmentAttendanceCatalog&,const PersonalAppointmentCatalog&);
// 生命周期限定在同次提案调用栈，原完整账先冷验；不得缓存引用或传入其他世界的承诺。
struct PersonalAppointmentAttendanceRead {
    const CommonStore& common;
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const PersonalAppointmentCatalog& catalog;
    const PersonalAppointmentState& state;
    const VisitAppointmentAttendanceRead* written_visits=nullptr;
    const VisitAppointmentAttendanceCatalog* written_catalog=nullptr;
    const PersonalAppointmentTime2Context* exact=nullptr;
};
Status validate_personal_appointment_attendance_sources(const PersonalAppointmentAttendanceRead&,const PersonalAppointmentAttendanceCatalog&);
// 仅原provider本人赴原已知地点；到场生成无Action等待，保留玩家真实开会的机会。
Outcome<std::optional<DecisionProposal>> propose_personal_appointment_attendance(
    const PersonalAppointmentAttendanceRead&,const PersonalAppointmentAttendanceCatalog&,
    const TravelCatalog&,const KnownSubjectView&,const DecisionProposal&);
// 只唤醒已由原Director登记的具名provider；不改公平游标或多给一次同tick决定。
Status wake_personal_appointment_attendance(const PersonalAppointmentAttendanceRead&,const PersonalAppointmentAttendanceCatalog&,const DirectorCatalog&,DirectorState&);
// 原计划可以保留已结束来源，但不能把另一个人的约定改作自己的永久承诺来源。
Status validate_personal_appointment_attendance_plan(const CommonStore&,const PersonalAppointmentState&,
    const PersonalAppointmentAttendanceCatalog*,const FinitePlan&);
}
