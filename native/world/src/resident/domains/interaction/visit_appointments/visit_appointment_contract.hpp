// 本人只读自己的承诺与真正已收的同一份邀请，发邀者不能由此得知远方领回/接受状态。
#pragma once
#include "visit_appointment_catalog.hpp"
namespace hanmo::resident {
// 三个原明确动作；日期由目录冻结，调用参数不准任意替双方选日子或改地点。
struct OfferVisitAppointment {StableId policy_id,message_fact_id;};
struct CollectVisitAppointment {StableId invitation_fact_id;};
struct AcceptVisitAppointment {StableId invitation_fact_id,collection_fact_id;};
using VisitAppointmentCommand=std::variant<OfferVisitAppointment,CollectVisitAppointment,AcceptVisitAppointment>;
enum class VisitAppointmentPerspective { inviter,invitee };
struct KnownVisitAppointment {
    WorldIdentity source_identity;
    StableId observer_actor_id,invitation_fact_id,policy_id,message_fact_id;
    VisitAppointmentPerspective perspective=VisitAppointmentPerspective::invitee;
    VisitAppointmentTerms terms;
    // 发邀者永远没有以下远方收件字段；收件者仅保自己的真实实领/接受来源。
    StableId collection_fact_id,acceptance_fact_id;
    // 仅本人原承诺/实领与实际共同会面能改变此状态，永不显示另一端远方领回/答受。
    StableId known_status,meeting_fact_id;
    Integer learned_tick=0;
};
// 完整三源、原接待/人物/控制/实际结果全验后派生，不读当前远方位置或把目录当已送达。
class VisitAppointmentRead {
public:
    virtual ~VisitAppointmentRead()=default;
    virtual Outcome<std::optional<KnownVisitAppointment>> known_visit_appointment(const StableId& observer,const StableId& invitation_fact_id) const=0;
};
// 赴约桥将分别消费发邀人的原offered承诺与收件人的原accepted承诺；不得看对方未知状态。
struct VisitAppointmentOwnCommitment {
    WorldIdentity source_identity;
    StableId actor_id,invitation_fact_id,commitment_fact_id,other_actor_id,place_id;
    Integer committed_tick=0,earliest_tick=0,latest_tick=0,duration_ticks=1;
};
}
