// 严格原命令边界只收policy_id，报价仅显示对象和一期占用，不在预览泄露答案。
#pragma once
#include "contact_inquiry.hpp"
#include "contact_inquiry_receipts.hpp"
namespace hanmo::resident {
struct ActionCompletion;
Outcome<ContactInquiryRequest> decode_contact_inquiry_command(const StableId& kind,const BoundaryValue& params);
BoundaryValue encode_contact_inquiry_command(const ContactInquiryRequest&);
BoundaryValue encode_contact_inquiry_quote(const ContactInquiryQuote&);
ActionCompletion contact_inquiry_completion(const ContactInquiryCompletion&);
Outcome<Integer> decode_contact_inquiry_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<bool> decode_contact_inquiry_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_contact_inquiry_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const ContactInquiryCatalog*,const ContactInquiryState*);
// 共同JSON全部具名头和活动params都取这份独立原件；不可导入时丢掉未知答案字段。
Outcome<BoundaryValue> contact_inquiry_fact_payloads(const CommonStore&,const ContactInquiryState*);
Outcome<BoundaryValue> contact_inquiry_action_payloads(const CommonStore&,const ContactInquiryState*);
Status validate_contact_inquiry_json_payloads(const CommonStore&,const ContactInquiryState*,const BoundaryValue& source);
}
