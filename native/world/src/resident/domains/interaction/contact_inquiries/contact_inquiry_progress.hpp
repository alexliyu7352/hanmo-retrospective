// 本人Continue仅消费已实际启动的原探听，不因有题目或同场人物就冒已开始；不在进度口提前透露答案。
#pragma once
#include "contact_inquiry_state.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct ContactInquiryTime2Context;
struct ContactInquiryActionSource {
    WorldIdentity source_identity;
    StableId actor_id,policy_id,action_id,started_fact_id,finished_fact_id,status,reason;
    ContactInquiryContact contact;
    Integer start_tick=0,remaining_ticks=0,finished_tick=-1;
    bool success=false;
    std::optional<WorldMoment> actual_started_at,actual_finished_at;
};
// 调用前同World完整来源与结果绑定先验；缺源不是普通旅行，别人Action保持未知。
Outcome<std::optional<ContactInquiryActionSource>> query_contact_inquiry_action(const CommonStore&,const ContactInquiryState&,const StableId& actor_id,const StableId& action_id,const ContactInquiryTime2Context* exact=nullptr);
// 成功固定ContactInquiryAnswered，失败原reason；必须本次实际Advance逐Action完成，没有混合领域载荷。
Outcome<std::optional<ContactInquiryActionSource>> query_contact_inquiry_completion(const CommonStore&,const ContactInquiryState&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer completed_tick,const StableId& actual_result_id,const ContactInquiryTime2Context* exact=nullptr);
}
