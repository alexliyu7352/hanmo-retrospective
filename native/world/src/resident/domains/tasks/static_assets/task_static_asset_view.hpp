// 本人任务实物历史只读已验证原凭据；不授继产资格、库存权限或组织全员知情。
#pragma once
#include "task_static_asset.hpp"
namespace hanmo::resident {
// 本人原承包凭据每页最多16条；每条批次也独立分页，避免无限展开原货物表。
Outcome<BoundaryValue> query_task_static_assets(const CommonStore&,const TaskState&,const TaskCatalog&,const TaskStaticAssetState&,Integer cursor=0,const StableId& receipt_id={},Integer lot_cursor=0);
}
