// 私人延期的World薄接头只借同候选原Task与Interaction；永久状态仍归Task原保存集。
#pragma once
#include "task_extension_view.hpp"
namespace hanmo::resident {
struct WorldBundle;
Status validate_task_extension_world(const WorldBundle&);
Outcome<TaskExtensionQuote> prepare_task_extension_world(const WorldBundle&,const StableId& actor,const TaskExtensionCommand&);
Outcome<InteractionEffect> commit_task_extension_world(WorldBundle&,const StableId& actor,const TaskExtensionCommand&,const TaskExtensionQuote&);
Outcome<BoundaryValue> query_task_extension_world(const WorldBundle&,Integer cursor=0);
BoundaryValue encode_task_extension_quote(const TaskExtensionQuote&);
}
