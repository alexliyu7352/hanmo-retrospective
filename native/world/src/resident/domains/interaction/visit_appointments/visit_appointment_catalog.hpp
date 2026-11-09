// 作者明确发邀承诺；只复用原私人会面的地点/本人/到场say，不修改旧当面预约条款。
#pragma once
#include "visit_appointment_state.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply.hpp"
#include "resident/domains/interaction/appointments/personal_appointment.hpp"
namespace hanmo::resident {
struct VisitAppointmentPolicy {
    StableId id,message_policy_id,appointment_policy_id,offer_node_id,collect_node_id;
    bool inviter_commits_fixed_window=false;
    Integer accept_before_offset=1,earliest_offset=1,latest_offset=1;
};
struct VisitAppointmentCatalog {Integer version=1;std::vector<VisitAppointmentPolicy> policies;};
// 一份原拜帖只能选择普通答复或新约期邀请目录，不能把旧contact_in_person偷偷加上日期。
Status validate_visit_appointment_catalog(const VisitAppointmentCatalog&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const InteractionCatalog&,const VisitReplyCatalog* ordinary_replies=nullptr);
Outcome<VisitAppointmentCatalog> decode_visit_appointment_catalog(const BoundaryValue&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const InteractionCatalog&,const VisitReplyCatalog* ordinary_replies=nullptr);
BoundaryValue encode_visit_appointment_catalog(const VisitAppointmentCatalog&);
const VisitAppointmentPolicy* visit_appointment_policy(const VisitAppointmentCatalog&,const StableId&);
}
