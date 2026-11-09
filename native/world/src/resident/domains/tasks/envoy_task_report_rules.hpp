// 返报沿同一Task与Common Action办理；没有新旅行钟、酬金或提议同意副作用。
#pragma once
#include "envoy_task_rules.hpp"
#include "envoy_task_report_contact.hpp"
#include "envoy_task_report_reference.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
namespace hanmo::resident {
// 政治域核原报告Office/site当前合法接收人及专项Grant，已递交的提议过期不撤销报告责任。
using EnvoyReportAuthorization=std::function<Status(const EnvoyTaskAcceptance&,const EnvoyTaskDelivery&,const FaceToFaceContactProof&)>;
struct EnvoyReportQuote {WorldIdentity source_identity;StableId task_id;Integer duration_ticks=0;FaceToFaceContactProof contact;};
Outcome<EnvoyReportQuote> preview_envoy_report(const CommonStore&,const TaskState&,const EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& actor_id,const StableId& task_id,const StableId& provider_actor_id,Integer expected_world_revision,const EnvoyReportAuthorization&);
// 报价与实际开工同源复核；失败须由外层撤回Common及所有相关领域候选。
Outcome<StableId> begin_envoy_report_in_candidate(CommonStore&,const TaskState&,EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& actor_id,const StableId& task_id,const StableId& provider_actor_id,Integer expected_world_revision,const EnvoyReportAuthorization&);
using EnvoyReportCommit=std::function<Outcome<DiplomaticEnvoyReportAttempt>(const TaskEnvoyReportContactRead&,const StableId& task_id,const StableId& action_id,Integer completed_tick)>;
// Action减零后、统一释放前调用；正常待接触/责任阻塞交任务阶段，只有真实政治报告才写唯一final。
Outcome<DiplomaticEnvoyReportAttempt> finish_envoy_report_in_candidate(CommonStore&,TaskState&,EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DiplomaticEnvoyReportRead&,const StableId& task_id,const StableId& action_id,Integer completed_tick,const EnvoyReportCommit&);
}
