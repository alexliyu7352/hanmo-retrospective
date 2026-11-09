// 新执行人取消使用独立typed命令与实际result绑定；旧proprietor cancel语义不扩展。
#pragma once
#include "task_shop_order_estate.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
inline constexpr const char* task_shop_order_estate_cancel_command="task_shop_order_estate_cancel";
Outcome<TaskShopOrderEstateCancel> decode_task_shop_order_estate_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_task_shop_order_estate_command(const TaskShopOrderEstateCancel&);
BoundaryValue encode_task_shop_order_estate_quote(const TaskShopOrderEstateCancellationQuote&);
// 仅原envelope本人采用复核四ID，效果指向新执行事实，不复用旧proprietor取消回执。
Outcome<InteractionEffect> commit_task_shop_order_estate_business_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,const PrivateShopOrderRead&,const PrivateShopEstateCancellationRead&,const StableId& actor,const TaskShopOrderEstateCancel&,const TaskShopOrderEstateCancellationQuote&,const TaskShopOrderExecutionCapture&);
// 本片仅玩家逐单取消；旧行永久绑定必须逐值保持，只收本次唯一新源。
Outcome<StableId> task_shop_order_estate_new_player_source(const TaskShopOrderEstateCancellationState* before,const TaskShopOrderEstateCancellationState* after,const StableId& actor,const StableId& command_kind,const ReceiptDetails&,Integer tick);
Status bind_task_shop_order_estate_player_result(CommonStore&,TaskShopOrderEstateCancellationState&,const StableId& source_fact_id,const StableId& actor,Integer request_sequence,const CommitResult& actual_result);
// 完整旧Task/新执行权验证后每条取消必须有真实玩家绑定；提交尾前的短暂空值不可冷存。
Status validate_task_shop_order_estate_cached_results(const CommonStore&,const TaskShopOrderEstateCancellationState*);
}
