// 首片仅玩家发起探听；真实请求及Advance完成在Common结果生成后永久绑定，无NPC空绑定例外。
#pragma once
#include "contact_inquiry_state.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Outcome<std::vector<StableId>> contact_inquiry_new_player_sources(const ContactInquiryState* before,const ContactInquiryState* after,const StableId& actor,const StableId& command_kind,const ReceiptDetails&,Integer tick);
Status bind_contact_inquiry_player_result(CommonStore&,ContactInquiryState&,const std::vector<StableId>& sources,const StableId& actor,Integer request_sequence,const CommitResult&);
// 完整原开工/答复源先验后核本口；未启用必须拒具名新回执，裁剪history不删除永久业务绑定。
Status validate_contact_inquiry_cached_results(const CommonStore&,const ContactInquiryState*);
}
