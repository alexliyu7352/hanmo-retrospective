// 当面询问只持原开工和实际答复；不存当前位置，不将尚未完成的探听当作已获知。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/interaction/s3_leaf_life.hpp"
#include "resident/domains/interaction/s3_event_time.hpp"
#include "resident/domains/interaction/s3_player_receipt_memory.hpp"
#include "resident/domains/interaction/known_contacts/known_contact_location_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* contact_inquiry_module="contact_inquiries";
inline constexpr const char* contact_inquiry_started_kind="contact_inquiry_started";
inline constexpr const char* contact_inquiry_finished_kind="contact_inquiry_finished";
inline constexpr const char* contact_inquiry_action_kind="contact_inquiry";
// 原私人说话者可明确分享本人最后亲见或婉拒；不接受任意对象参数、代理或组织权限推定。
struct ContactInquiryPolicy {StableId id,provider_actor_id,subject_actor_id,role_id,dialogue_id,node_id,disclosure;Integer duration_ticks=1;};
struct ContactInquiryCatalog {Integer version=1;std::vector<ContactInquiryPolicy> policies;};
struct ContactInquiryContact {StableId actor_id,provider_actor_id,session_id,place_id,role_id,dialogue_id,node_id;Integer tick=0;};
// 真正Common结果生成后绑定，首history裁剪后也不能把旧探听冒成本次完成。
struct ContactInquiryPlayerReceipt {
 CheckpointCopyLease copied_fields; // runtime ownership, never a wire field
 ContactInquiryPlayerReceipt()=default;
 explicit ContactInquiryPlayerReceipt(const S3PlayerReceiptBuildView&);
 ContactInquiryPlayerReceipt(std::string_view result,std::string_view actor,Integer sequence,Integer revision);
 ContactInquiryPlayerReceipt(const ContactInquiryPlayerReceipt&);
 ContactInquiryPlayerReceipt(ContactInquiryPlayerReceipt&&);
 ContactInquiryPlayerReceipt& operator=(const ContactInquiryPlayerReceipt&);
 ContactInquiryPlayerReceipt& operator=(ContactInquiryPlayerReceipt&&);
 void swap(ContactInquiryPlayerReceipt&)noexcept;
 friend void swap(ContactInquiryPlayerReceipt&a,ContactInquiryPlayerReceipt&b)noexcept{a.swap(b);}
 StableId result_id,request_actor_id;Integer request_sequence=0,result_revision=0;
};
struct ContactInquiryStarted {StableId fact_id,policy_id,action_id;ContactInquiryContact contact;std::optional<ContactInquiryPlayerReceipt> player_receipt;};
// reply_kind仅known/unknown/declined；失败为空，无新知情。answer仍是提供者原亲见，不借当前角色/坐标。
struct ContactInquiryFinished {StableId fact_id,start_fact_id,action_id;Integer tick=0;bool success=false;StableId reason,reply_kind;std::optional<KnownContactLocation> answer;std::optional<ContactInquiryPlayerReceipt> player_receipt;};
enum class ContactInquiryLifeRole {inquirer,provider};
struct ContactInquiryLifeSource {DeathRef death_ref;StableId start_fact_id;ContactInquiryLifeRole role=ContactInquiryLifeRole::inquirer;S3LeafLifeEffect effect;};
struct ContactInquiryState {
    Integer version=1;
    OrderedTable<ContactInquiryStarted> started;
    OrderedTable<ContactInquiryFinished> finished;
    OrderedTable<ContactInquiryLifeSource> life_sources;
    OrderedTable<S3EventTime> temporal_events;
    CheckpointMap<StableId,std::vector<StableId>> by_listener;
};
void rebuild_contact_inquiry_indexes(ContactInquiryState&);
Outcome<ContactInquiryState> decode_contact_inquiries(const BoundaryValue&);
BoundaryValue encode_contact_inquiries(const ContactInquiryState&);
Status visit_contact_inquiries_checkpoint(CodecVisitor&,ContactInquiryState&);
BoundaryValue encode_contact_inquiry_action(const ContactInquiryStarted&);
Outcome<BoundaryValue> encode_contact_inquiry_fact(const ContactInquiryState&,const StableId& fact_id);
}
