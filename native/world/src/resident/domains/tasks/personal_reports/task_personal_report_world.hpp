// 返报薄桥自持原Q01工厂，全部会面/生命/控制来源借同一候选；原Task独占钱款与Action写入。
#pragma once
#include "task_personal_report_services.hpp"
#include "task_personal_report_profile.hpp"
#include "task_personal_report_receipts.hpp"
#include "task_personal_report_view.hpp"
#include "task_personal_report_progress.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
namespace hanmo::resident {
struct WorldBundle;class CharacterConditionWorldRead;
class TaskPersonalReportWorldAccess final {
public:
 static Outcome<std::shared_ptr<TaskPersonalReportWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr,const CharacterConditionWorldRead* original_body=nullptr);
 TaskPersonalReportWorldAccess(const TaskPersonalReportWorldAccess&)=delete;
 TaskPersonalReportWorldAccess& operator=(const TaskPersonalReportWorldAccess&)=delete;
 const TaskPersonalReportServices& services()const{return services_;}
private:
 TaskPersonalReportWorldAccess(const WorldBundle&,std::shared_ptr<TaskPersonalMessageWorldAccess>);
 std::shared_ptr<TaskPersonalMessageWorldAccess> delivery_;
 TaskPersonalReportServices services_;
};
Status validate_task_personal_report_world(const WorldBundle&,const CharacterConditionWorldRead* original_body=nullptr);
Status task_personal_report_npc_command_allowed(const StableId& kind);
Outcome<TaskPersonalReportQuote> preview_task_personal_report_world(const WorldBundle&,const StableId&,const TaskPersonalReport&,const CharacterConditionWorldRead* original_body=nullptr);
Status revalidate_task_personal_report_quote_world(const WorldBundle&,const StableId&,const TaskPersonalReport&,const TaskPersonalReportQuote&,const CharacterConditionWorldRead* original_body=nullptr);
Status start_task_personal_report_world(WorldBundle&,const ActionCoreView&,const TaskPersonalReportQuote&,const TaskExecutionServices*,const CharacterConditionWorldRead* original_body=nullptr);
Outcome<BoundaryValue> query_task_personal_reports_world(const WorldBundle&,Integer cursor=0,const CharacterConditionWorldRead* original_body=nullptr);
struct TaskPersonalReportResultBatch {Integer before_tick=0;std::vector<StableId> facts;};
Outcome<TaskPersonalReportResultBatch> task_personal_report_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& committed_kind,const ReceiptDetails&);
Status bind_task_personal_report_result_world(WorldBundle&,const TaskPersonalReportResultBatch&,const StableId& actor,Integer request,const CommitResult&,const CharacterConditionWorldRead* original_body=nullptr);
}
