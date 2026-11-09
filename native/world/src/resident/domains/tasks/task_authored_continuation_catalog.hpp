// 作者只给已受领信件的原岗位地址；启用续行不授未知委托或新人物知识。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct TaskCatalog;
struct TaskMessageAddress { StableId offer_id,recipient_role_id; };
// v2额外允许已受领治理/使者的有界续行；v1不自动开启新事业或新任务。
struct TaskAuthoredContinuationCatalog { Integer version=1;std::vector<TaskMessageAddress> message_addresses; };
// 独立可选目录严格解码；null由Director旧目录保持，不在旧冷状态追加字段。
Outcome<TaskAuthoredContinuationCatalog> decode_task_authored_continuation_catalog(const BoundaryValue&);
// 每个地址须指向原message收件人自己的办理岗位；不能借另一岗位追踪现任者。
Status validate_task_authored_continuation_catalog(const TaskAuthoredContinuationCatalog&,const TaskCatalog&,const CommonCatalog&);
}
