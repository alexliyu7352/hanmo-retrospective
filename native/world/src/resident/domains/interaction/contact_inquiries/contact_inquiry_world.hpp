// 同候选真实原亲见、人物存在/死因与询问表；无第二世界或跨提交借用。
#pragma once
#include "contact_inquiry_boundary.hpp"
#include "contact_inquiry_view.hpp"
#include "contact_inquiry_progress.hpp"
namespace hanmo::resident {
struct WorldBundle;
Status validate_contact_inquiry_world(const WorldBundle&,const ContactInquiryTime2Context* exact=nullptr);
Outcome<ContactInquiryQuote> prepare_contact_inquiry_world(const WorldBundle&,const StableId& actor,const ContactInquiryRequest&,const ContactInquiryTime2Context* exact=nullptr);
Outcome<StableId> commit_contact_inquiry_world(WorldBundle&,const StableId& actor,const ContactInquiryRequest&,const ContactInquiryQuote&,const ContactInquiryTime2Context* exact=nullptr);
Outcome<ContactInquiryCompletion> step_contact_inquiry_world(WorldBundle&,const ActionCoreView&,const DynamicStepInput&);
Outcome<BoundaryValue> query_contact_inquiry_world(const WorldBundle&,Integer cursor=0,const ContactInquiryTime2Context* exact=nullptr);
Outcome<std::vector<StableId>> contact_inquiry_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& kind,const ReceiptDetails&);
Outcome<std::optional<ContactInquiryActionSource>> query_contact_inquiry_action_world(const WorldBundle&,const StableId& actor,const StableId& action,const ContactInquiryTime2Context* exact=nullptr);
Outcome<std::optional<ContactInquiryActionSource>> query_contact_inquiry_completion_world(const WorldBundle&,const StableId& actor,const StableId& action,const ReceiptDetails&,Integer completed_tick,const StableId& actual_result_id,const ContactInquiryTime2Context* exact=nullptr);
Status bind_contact_inquiry_result_world(WorldBundle&,const std::vector<StableId>& sources,const StableId& actor,Integer sequence,const CommitResult&);
}
