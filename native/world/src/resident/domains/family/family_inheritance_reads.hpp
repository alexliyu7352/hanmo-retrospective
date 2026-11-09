// 遗产只消费各原责任域的具名同候选纯读，不自行解释Task、工资、建造或财政内部账。
#pragma once
#include "family_inheritance_commands.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice.hpp"
#include "resident/domains/local_economy/private_account_history.hpp"
namespace hanmo::resident {
// 适配器保永久原信ID，当前identity来自本次S3实际读取，不缓存旧World版本。
FamilyReceivedDeathNoticeRead family_received_death_notice_read(const TaskDeliveredDeathNoticeRead&);
// context、Task和回调全部只借本次World候选；工厂仅用于已完成各原域完整验证之后。
InheritancePrivateClearanceRead family_inheritance_private_clearance_read(const EconomicPrivateHistoryContext&,const TaskState&,const TaskCatalog&);
}
