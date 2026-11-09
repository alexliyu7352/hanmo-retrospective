// 作者必须显式选定文书交接凭据；安装其他内容包的能力不能替旧情景补收件历史。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
Outcome<Integer> decode_task_message_handoff_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<bool> decode_task_message_handoff_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
}
