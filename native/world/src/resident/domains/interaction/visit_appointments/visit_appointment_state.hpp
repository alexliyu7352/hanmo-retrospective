// 新明确会面邀请独立于旧contact_in_person回帖；托回、本人实领和本人接受三种事实不能互代。
#pragma once
#include "resident/domains/interaction/visit_messages/visit_message_state.hpp"
#include "resident/domains/interaction/s3_leaf_life.hpp"
#include "resident/domains/interaction/s3_player_receipt_memory.hpp"
namespace hanmo::resident {
inline constexpr const char* visit_appointment_module="visit_appointment_invitations";
inline constexpr const char* visit_appointment_offered_kind="visit_appointment_invitation_offered";
inline constexpr const char* visit_appointment_collected_kind="visit_appointment_invitation_collected";
inline constexpr const char* visit_appointment_accepted_kind="visit_appointment_invitation_accepted";
struct VisitAppointmentPlayerReceipt {
 CheckpointCopyLease copied_fields; // runtime ownership, never a wire field
 VisitAppointmentPlayerReceipt()=default;
 explicit VisitAppointmentPlayerReceipt(const S3PlayerReceiptBuildView&);
 VisitAppointmentPlayerReceipt(std::string_view result,std::string_view actor,Integer sequence,Integer revision);
 VisitAppointmentPlayerReceipt(const VisitAppointmentPlayerReceipt&);
 VisitAppointmentPlayerReceipt(VisitAppointmentPlayerReceipt&&);
 VisitAppointmentPlayerReceipt& operator=(const VisitAppointmentPlayerReceipt&);
 VisitAppointmentPlayerReceipt& operator=(VisitAppointmentPlayerReceipt&&);
 void swap(VisitAppointmentPlayerReceipt&)noexcept;
 friend void swap(VisitAppointmentPlayerReceipt&a,VisitAppointmentPlayerReceipt&b)noexcept{a.swap(b);}
 StableId result_id,request_actor_id;Integer request_sequence=0,result_revision=0;
};
// NPC必须另有完整显式意向和原真实接待认领；格式支持不是准许无producer写入。
struct VisitAppointmentAuthoredOrigin {StableId intent_id,reception_claim_fact_id;};
struct VisitAppointmentTerms {
    StableId appointment_policy_id,inviter_actor_id,invitee_actor_id,place_id,role_id;
    Integer offered_tick=0,accept_before_tick=0,earliest_tick=0,latest_tick=0,duration_ticks=1;
};
// 原收帖人亲自托回邀请时即承诺其固定窗口；其知情范围不含远方领回/接受状态。
struct VisitAppointmentOffered {
    StableId fact_id,policy_id,message_fact_id,received_fact_id;
    VisitMessageContact contact;
    VisitAppointmentTerms terms;
    std::optional<VisitAppointmentPlayerReceipt> player_receipt;
    std::optional<VisitAppointmentAuthoredOrigin> authored_origin;
};
// 回帖收件人即原拜帖发信者，必须从原接待者处亲自实领，不由原发邀者或全局索引代知。
struct VisitAppointmentCollected {
    StableId fact_id,invitation_fact_id;
    VisitMessageContact contact;
    std::optional<VisitAppointmentPlayerReceipt> player_receipt;
};
// 领回不等于接受；本人再明确接受同一份未过期条款，不重算窗口或改写发邀人的原承诺。
struct VisitAppointmentAccepted {
    StableId fact_id,invitation_fact_id,collection_fact_id,actor_id;
    Integer tick=0;
    std::optional<VisitAppointmentPlayerReceipt> player_receipt;
};
enum class VisitAppointmentLifeRole {inviter,invitee};
struct VisitAppointmentLifeSource {DeathRef death_ref;StableId invitation_fact_id;VisitAppointmentLifeRole role=VisitAppointmentLifeRole::inviter;S3LeafLifeEffect effect;};
struct VisitAppointmentState {
    Integer version=1;
    OrderedTable<VisitAppointmentOffered> offered;
    OrderedTable<VisitAppointmentCollected> collected;
    OrderedTable<VisitAppointmentAccepted> accepted;
    OrderedTable<VisitAppointmentLifeSource> life_sources;
};
Status visit_visit_appointments_checkpoint(CodecVisitor&,VisitAppointmentState&);
BoundaryValue encode_visit_appointments(const VisitAppointmentState&);
Outcome<VisitAppointmentState> decode_visit_appointments(const BoundaryValue&);
Outcome<BoundaryValue> encode_visit_appointment_fact(const VisitAppointmentState&,const StableId& fact_id);
}
