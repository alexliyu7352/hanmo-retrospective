// 私人预约保存双方当面达成的约期；不复制人物位置、不预占未来Action，也不授教学或财产权限。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/interaction/s3_leaf_life.hpp"
#include "resident/domains/interaction/s3_event_time.hpp"
#include "resident/domains/interaction/s3_player_receipt_memory.hpp"
namespace hanmo::resident {
inline constexpr const char* personal_appointment_module="personal_appointments";
inline constexpr const char* personal_appointment_accepted_kind="personal_appointment_accepted";
inline constexpr const char* personal_appointment_cancelled_kind="personal_appointment_cancelled";
inline constexpr const char* personal_appointment_started_kind="personal_appointment_started";
inline constexpr const char* personal_appointment_finished_kind="personal_appointment_finished";
inline constexpr const char* personal_appointment_action_kind="personal_appointment_meet";
// 三个节点分别只表示本人答允、当面取消和实际开始会面；不会从普通招呼猜出同意。
struct PersonalAppointmentPolicy {
    StableId id,provider_actor_id,role_id,dialogue_id,accept_node_id,cancel_node_id,meet_node_id;
    Integer earliest_offset=1,latest_offset=1,duration_ticks=1;
};
struct PersonalAppointmentCatalog {Integer version=1;std::vector<PersonalAppointmentPolicy> policies;};
// 保存原说话者和原岗位初始身份；后继换班不改过去本人承诺。
struct PersonalAppointmentContact {
    StableId actor_id,provider_actor_id,session_id,place_id,role_id,dialogue_id,node_id;
    Integer tick=0;
};
// 原提交结果分配后绑定；只证明这次真实请求，不另造一份预约/终结账。
struct PersonalAppointmentPlayerReceipt {
 CheckpointCopyLease copied_fields; // runtime ownership, never a wire field
 PersonalAppointmentPlayerReceipt()=default;
 explicit PersonalAppointmentPlayerReceipt(const S3PlayerReceiptBuildView&);
 PersonalAppointmentPlayerReceipt(std::string_view result,std::string_view actor,Integer sequence,Integer revision);
 PersonalAppointmentPlayerReceipt(const PersonalAppointmentPlayerReceipt&);
 PersonalAppointmentPlayerReceipt(PersonalAppointmentPlayerReceipt&&);
 PersonalAppointmentPlayerReceipt& operator=(const PersonalAppointmentPlayerReceipt&);
 PersonalAppointmentPlayerReceipt& operator=(PersonalAppointmentPlayerReceipt&&);
 void swap(PersonalAppointmentPlayerReceipt&)noexcept;
 friend void swap(PersonalAppointmentPlayerReceipt&a,PersonalAppointmentPlayerReceipt&b)noexcept{a.swap(b);}
 StableId result_id,request_actor_id;Integer request_sequence=0,result_revision=0;
};
struct PersonalAppointmentAccepted {
    StableId fact_id,policy_id;
    PersonalAppointmentContact contact;
    Integer earliest_tick=0,latest_tick=0,duration_ticks=1;
    std::optional<PersonalAppointmentPlayerReceipt> player_receipt;
};
// 首版只有双方再次实际在场的取消；单方远程离开不会伪造已送达的变更通知。
struct PersonalAppointmentCancelled {
    StableId fact_id,appointment_id;
    PersonalAppointmentContact contact;
    std::optional<PersonalAppointmentPlayerReceipt> player_receipt;
};
// 开工事实永久保留，实际占用仍由同ID Common Action唯一承担。
struct PersonalAppointmentStarted {
    StableId fact_id,appointment_id,action_id;
    PersonalAppointmentContact contact;
    std::optional<PersonalAppointmentPlayerReceipt> player_receipt;
};
struct PersonalAppointmentFinished {
    StableId fact_id,appointment_id,start_fact_id,action_id;
    Integer tick=0;
    bool success=false;
    StableId reason;
    std::optional<PersonalAppointmentPlayerReceipt> player_receipt;
};
enum class PersonalAppointmentLifeRole {inviter,invitee};
struct PersonalAppointmentLifeSource {DeathRef death_ref;StableId appointment_id;PersonalAppointmentLifeRole role=PersonalAppointmentLifeRole::inviter;S3LeafLifeEffect effect;};
struct PersonalAppointmentState {
    Integer version=1;
    OrderedTable<PersonalAppointmentAccepted> accepted;
    OrderedTable<PersonalAppointmentCancelled> cancelled;
    OrderedTable<PersonalAppointmentStarted> started;
    OrderedTable<PersonalAppointmentFinished> finished;
    OrderedTable<PersonalAppointmentLifeSource> life_sources;
    OrderedTable<S3EventTime> temporal_events;
    // 派生索引不持久化；收件人仅因自己当面答允而可知，不扫描其他人的预约。
    CheckpointMap<StableId,std::vector<StableId>> by_participant;
};
void rebuild_personal_appointment_indexes(PersonalAppointmentState&);
Outcome<PersonalAppointmentState> decode_personal_appointments(const BoundaryValue&);
BoundaryValue encode_personal_appointments(const PersonalAppointmentState&);
// 导出原Common工单/事实完整载荷；导入应与已恢复独立段逐值比较，不重造来源。
BoundaryValue encode_personal_appointment_action(const PersonalAppointmentStarted&);
Outcome<BoundaryValue> encode_personal_appointment_fact(const PersonalAppointmentState&,const StableId& fact_id);
Status visit_personal_appointments_checkpoint(CodecVisitor&,PersonalAppointmentState&);
}
