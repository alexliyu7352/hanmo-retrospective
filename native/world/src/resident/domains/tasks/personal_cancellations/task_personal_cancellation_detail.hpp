// 仅本次原私人销约的双序与稳定边界；不把当前角色或库存回填为过去许可。
#pragma once
#include "task_personal_cancellation_contract.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_detail.hpp"
namespace hanmo::resident::personal_cancellation_detail {
using personal_message_detail::serial;using personal_message_detail::before;using personal_message_detail::identity;using personal_message_detail::narrative;
inline const TaskPersonalCancellationRecord* for_task(const TaskPersonalCancellationState& state,const StableId& task){const TaskPersonalCancellationRecord* found=nullptr;for(const auto& row:state.cancellations.rows())if(row.second.task_id==task){if(found)return nullptr;found=&row.second;}return found;}
inline const TaskStaticAssetReceipt* original_return(const TaskStaticAssetState& state,const StableId& task){const TaskStaticAssetReceipt* found=nullptr;for(const auto& row:state.receipts.rows())if(row.second.task_id==task&&row.second.stage=="cancellation_return"){if(found)return nullptr;found=&row.second;}return found;}
}
