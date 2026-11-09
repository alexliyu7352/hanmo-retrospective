// 显式handoff4仅配本段；保旧Task控制与Q01声明的同一借用实例，不调用World全验。
#include "task_personal_report_services.hpp"
namespace hanmo::resident {
Status validate_task_personal_report_services(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const TaskPersonalReportState* state,const TaskPersonalReportServices* service){if(bool(state)!=(service!=nullptr))return common_failure("TaskPersonalReportServicesMismatch");if(!service)return common_success();const auto& x=service->sources;const auto& d=x.delivery;if(state!=&x.state||&c!=&d.common||&tasks!=&d.tasks||&catalog!=&d.task_catalog||!tasks.message_handoffs||&*tasks.message_handoffs!=&d.handoffs||d.handoffs.version!=4||!tasks.personal_messages||&*tasks.personal_messages!=&d.state)return common_failure("TaskPersonalReportCandidateRequired");return common_success();}
Status validate_task_personal_report_extension_pair(const TaskPersonalReportCatalog& reports,const TaskExtensionCatalog* extensions){if(extensions)for(const auto& p:reports.policies)for(const auto& e:extensions->policies)if(p.offer_id==e.offer_id)return common_failure("TaskPersonalReportDeadlineExtensionUnsupported",p.offer_id);return common_success();}
}
