// 显式独立段与原托留模块配对；只收两具名来源，不能用未知Common头装作回信。
#pragma once
#include "visit_reply_view.hpp"
namespace hanmo::resident {
struct VisitReplyIntentCatalog;
bool visit_reply_command_kind(const StableId&);
// 原具名 profile 与实际目录共同决定 0/1/2/3/4；bool 入口只投影是否选中。
Outcome<Integer> decode_visit_reply_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<bool> decode_visit_reply_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_visit_reply_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const VisitReplyCatalog*,const VisitReplyState*,const VisitReplyIntentCatalog* intents=nullptr);
Outcome<BoundaryValue> visit_reply_fact_payloads(const CommonStore&,const VisitReplyState*);
Status validate_visit_reply_json_payloads(const CommonStore&,const VisitReplyState*,const BoundaryValue& source);
}
