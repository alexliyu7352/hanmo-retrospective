// 仅所选情景的显式外交块共同开启政治账与使者责任，旧Common回执也必须守门。
#pragma once
#include "diplomacy_offers.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t diplomatic_report_checkpoint_feature=2048u;
inline Status validate_diplomacy_runtime_profile(const DiplomacyCatalog& catalog){
    if((catalog.version!=1&&catalog.version!=2)||(catalog.version==2)!=catalog.v2.has_value())return common_failure("DiplomacyVersionMismatch");
    if(!catalog.enabled)return common_failure("DiplomacyProfileRequired");
    for(const auto& row:catalog.mission_policies.rows())if(catalog.version==1&&row.second.task_terms&&row.second.task_terms->return_report_required)return common_failure("DiplomaticReturnReportUnsupported",row.first);
    return common_success();
}
inline Status validate_diplomacy_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,bool political_state,bool envoy_state,Integer version=1){
    if(enabled&&version!=1&&version!=2)return common_failure("UnsupportedDiplomacyModuleVersion");
    unsigned politics=0,envoy=0;
    for(const auto& module:modules)if(module.first=="diplomacy"||module.first=="envoy_tasks"){
        if(module.second!=std::to_string(version))return common_failure("UnsupportedDiplomacyModuleVersion",module.first);
        auto& count=module.first=="diplomacy"?politics:envoy;if(++count!=1)return common_failure("DuplicateDiplomacyModule",module.first);
    }
    if(!enabled)return politics||envoy||political_state||envoy_state?common_failure("UnexpectedDiplomacyProfile"):common_success();
    if(politics!=1||envoy!=1)return common_failure("IncompleteDiplomacyModules");
    if(!political_state||!envoy_state)return common_failure("MissingDiplomacyState");
    return common_success();
}
inline Status validate_diplomacy_command_history(const CommonStore& common,bool enabled,Integer version=1){
    const auto invalid_version=[&](const StableId& kind){return kind=="diplomatic_receive_envoy_report"||((kind=="envoy_report"||kind=="diplomatic_delegate_envoy")&&(!enabled||version!=2));};
    for(const auto& row:common.state().results.rows())if(row.second&&invalid_version(row.second->kind))return common_failure("DiplomaticReportVersionRequired","results/"+row.first);
    for(const auto& row:common.state().history)if(invalid_version(row.kind))return common_failure("DiplomaticReportVersionRequired","history/"+row.result_id);
    if(enabled)return common_success();
    const auto belongs=[](const StableId& kind){return kind.rfind("diplomatic_",0)==0||kind=="envoy_accept"||kind=="envoy_deliver"||kind=="envoy_cancel"||kind=="envoy_report";};
    for(const auto& row:common.state().results.rows())if(row.second&&belongs(row.second->kind))return common_failure("DiplomacyProfileRequired","results/"+row.first);
    for(const auto& row:common.state().history)if(belongs(row.kind))return common_failure("DiplomacyProfileRequired","history/"+row.result_id);
    return common_success();
}
}
