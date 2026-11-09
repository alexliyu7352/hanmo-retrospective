// 新提交来源只在完整候选尾绑定；早Shop/政治结构读不调用这个完整历史出口。
#pragma once
#include "resident/domains/commerce/command_results/private_shop_command_results.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct WorldBundle;
Status initialize_private_shop_command_results_world_in_candidate(WorldBundle&);
Status validate_private_shop_command_results_world(const WorldBundle&,ProjectionWriter* projection=nullptr);
// 必须放原Family/Legacy/Bio/弟子控制史全部通过之后；无借用对象跨调用保存。
Status validate_private_shop_command_result_player_world(const WorldBundle&,ProjectionWriter* projection=nullptr);
Status bind_private_shop_command_results_world(const WorldBundle* actual_before,WorldBundle&,const StableId&,Integer request_seq,const CommitResult& actual_result);
}
