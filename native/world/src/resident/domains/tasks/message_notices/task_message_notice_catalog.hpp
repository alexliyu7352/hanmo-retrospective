// 作者只指定已有私信委托的具名死亡主题；真实死亡、亲历与送达分别向其唯一领域读取。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
struct TaskDeathNoticeBindingPolicy {StableId offer_id,decedent_actor_id;};
struct TaskMessageNoticeCatalog {Integer version=1;OrderedTable<TaskDeathNoticeBindingPolicy> bindings;};
// 缺省不启用，能力并集不会替旧剧本制造通知；当前只收既有message offer，不另造任务。
Outcome<std::optional<TaskMessageNoticeCatalog>> decode_task_message_notice_catalog(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_task_message_notice_catalog(const TaskMessageNoticeCatalog&,const TaskCatalog&,const CommonCatalog&);
}
