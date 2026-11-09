#pragma once
#include "visit_appointment.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
// 构造前调用方持同World完整三源/永久结果全验；本类仅有限投影，不从当前位置推知邀请。
class VisitAppointmentAccess final:public VisitAppointmentRead {
public:
    VisitAppointmentAccess(const CommonStore& common,const VisitAppointmentState& state,const VisitAppointmentMeetingState* meetings=nullptr,const CurrentMomentRead* clock=nullptr):common_(common),state_(state),meetings_(meetings),clock_(clock){}
    Outcome<std::optional<KnownVisitAppointment>> known_visit_appointment(const StableId&,const StableId&) const override;
    Outcome<std::vector<VisitAppointmentOwnCommitment>> own_commitments(const StableId& actor,Integer from,Integer through) const;
private:const CommonStore& common_;const VisitAppointmentState& state_;const VisitAppointmentMeetingState* meetings_;const CurrentMomentRead* clock_;
};
}
