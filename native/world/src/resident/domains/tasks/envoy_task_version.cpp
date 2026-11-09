// 冷恢复必须同时核载荷与Common旧回执，不能删sidecar或降版本后保留返报效果。
#include "envoy_task_version.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_report_contract.hpp"
namespace hanmo::resident {
namespace {
// 公开命令与政治专项Grant分开；diplomatic_receive_envoy_report不是玩家命令。
bool envoy_command(const StableId& kind){return kind=="envoy_accept"||kind=="envoy_deliver"||kind=="envoy_cancel"||kind=="envoy_report";}
// 这些历史事实始终属于使者责任；具体逐源真实性仍交完整领域校验。
bool envoy_fact(const StableId& kind){return kind=="envoy_task_accepted"||kind=="envoy_task_delivered"||kind=="envoy_task_completed"||kind=="envoy_task_cancelled"||kind=="envoy_task_failed"||kind=="envoy_handover_interrupted";}
}
Status validate_envoy_task_version_boundary(const CommonStore& common,Integer version,const EnvoyTaskState* payload){
    if(version<0||version>2)return common_failure("UnsupportedEnvoyTaskVersion");
    if(version==0&&payload)return common_failure("UnexpectedEnvoyTaskDomain");
    if(version!=0&&(!payload||payload->version!=version))return common_failure("EnvoyTaskVersionMismatch");
    // 旧v1正式入口只支持无返报的本人使命；拒绝已有内部形状被坏档拿来激活未发布规则。
    if(version==1)for(const auto& row:payload->envoy_contracts.rows()){
        const auto& contract=row.second;
        if(contract.acceptance.return_report_required||(contract.action&&contract.action->kind==EnvoyTaskActionKind::report)||(contract.completion&&(contract.completion->report_action_id||contract.completion->diplomatic_report_fact_id)))return common_failure("EnvoyReportVersionRequired",row.first);
        for(const auto& old:contract.interruptions)if(old.action_kind=="envoy_report")return common_failure("EnvoyReportVersionRequired",row.first);
    }
    const auto receipt_allowed=[version](const StableId& kind){return kind!=diplomatic_envoy_report_command_kind&&(!envoy_command(kind)||version!=0)&&(kind!="envoy_report"||version==2);};
    for(const auto& row:common.state().results.rows())if(row.second&&!receipt_allowed(row.second->kind))return common_failure("EnvoyCommandVersionMismatch","results/"+row.first);
    for(const auto& row:common.state().history)if(!receipt_allowed(row.kind))return common_failure("EnvoyCommandVersionMismatch","history/"+row.result_id);
    for(const auto& row:common.state().actions.rows())if((version==0&&(row.second.kind=="envoy_deliver"||row.second.kind=="envoy_report"))||(version<2&&row.second.kind=="envoy_report"))return common_failure("EnvoyActionVersionMismatch",row.first);
    for(const auto& row:common.state().facts.rows())if((version==0&&envoy_fact(row.second.kind))||(version<2&&row.second.kind==diplomatic_envoy_report_fact_kind))return common_failure("EnvoyFactVersionMismatch",row.first);
    return common_success();
}
}
