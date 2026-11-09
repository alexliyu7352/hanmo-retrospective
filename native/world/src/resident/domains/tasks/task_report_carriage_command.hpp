#pragma once
#include "task_report_carriage.hpp"
namespace hanmo::resident {
using TaskReportCarriageCommand=std::variant<PostTaskReportCarriage,HandOverTaskReportCarriage>;
Outcome<TaskReportCarriageCommand> decode_task_report_carriage_command(const StableId&kind,const BoundaryValue&);
Status emit_task_report_carriage_command_fields(ProjectionWriter&,const TaskReportCarriageCommand&);
struct TaskReportCarriageQuote {
 CheckpointMemoryCharge memory;
 TaskReportCarriageQuote()=default;
 TaskReportCarriageQuote(const TaskReportCarriageQuote&)=delete;
 TaskReportCarriageQuote&operator=(const TaskReportCarriageQuote&)=delete;
 TaskReportCarriageQuote(TaskReportCarriageQuote&&)=default;
 TaskReportCarriageQuote&operator=(TaskReportCarriageQuote&&)=delete;
 StableId actor_id;TaskReportCarriageCommand command;WorldMoment quoted_at;
 std::optional<ReportDisclosurePermission> permission;
 std::optional<MomentOwnerSource> actual_contact;
 std::shared_ptr<const ReadStamp> stamp;
};
Outcome<TaskReportCarriageQuote> preview_task_report_carriage(const TaskReportCarriageContext&,const StableId&authenticated_actor,const TaskReportCarriageCommand&);
// The original outer candidate owns every write, including the Common result.
// Failure at any stage discards that candidate; no partial message escapes.
Outcome<StableId> apply_task_report_carriage_in_candidate(CommonStore&,TaskState&,SocialState&,CommunicationLinksState&,const TaskReportCarriageContext&,const StableId&authenticated_actor,const TaskReportCarriageQuote&);
}
