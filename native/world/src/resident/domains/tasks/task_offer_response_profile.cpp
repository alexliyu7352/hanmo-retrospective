// 新profile严格显式开启；旧任务与互动codec不调用本域，保原版本与字段序。
#include "task_offer_response_profile.hpp"
#include "task_offer_response.hpp"
#include "resident/storage/common_codec.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
// 所选情景是唯一开关；所装的另一包要求回应能力不会替此情景建立响应历史。
Outcome<bool> decode_task_offer_response_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;object(scenario,"scenario");const auto* profile=scenario.find("task_offer_response_profile");if(!profile)return {true,false,{}};
    if(string(*profile,"scenario/task_offer_response_profile")!="known_offers_v1")unsupported("UnsupportedTaskOfferResponseProfile","scenario/task_offer_response_profile");
    if(std::find(capabilities.begin(),capabilities.end(),"task_offer_responses_v1")==capabilities.end())unsupported("MissingTaskOfferResponseCapability","required_capabilities");
    const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)unsupported("TaskOfferResponseNativeProfileRequired","scenario/dynamic_world");
    const auto* native_profile=dynamic->find("profile");if(!native_profile||string(*native_profile,"scenario/dynamic_world/profile")!="local_society_v1")unsupported("TaskOfferResponseNativeProfileRequired","scenario/dynamic_world/profile");
    return {true,true,{}};
}catch(const boundary::DecodeFailure& error){return {false,false,error.error};}}
Outcome<Integer> decode_task_offer_response_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;object(scenario,"scenario");const auto* selected=scenario.find("task_offer_response_profile");if(!selected)return {true,0,{}};
    const auto profile=string(*selected,"scenario/task_offer_response_profile");
    if(profile=="known_offers_v1"){auto old=decode_task_offer_response_profile(scenario,capabilities);return old.ok?Outcome<Integer>{true,1,{}}:Outcome<Integer>{false,0,old.error};}
    if(profile!="known_offers_v2")unsupported("UnsupportedTaskOfferResponseProfile","scenario/task_offer_response_profile");
    // 保留原回应能力承载作者回应，动态机会沿既有原生/会话/多人物能力；不改全局摘要。
    for(const auto* capability:{"task_offer_responses_v1","native_dynamic_world","dialogue","multi_actor"})if(std::find(capabilities.begin(),capabilities.end(),capability)==capabilities.end())unsupported("MissingTaskOfferResponseCapability",capability);
    const auto* dynamic=scenario.find("dynamic_world");const auto* native=dynamic?dynamic->find("profile"):nullptr;
    if(!native||string(*native,"scenario/dynamic_world/profile")!="local_society_v1")unsupported("TaskOfferResponseNativeProfileRequired","scenario/dynamic_world/profile");
    return {true,2,{}};
}catch(const boundary::DecodeFailure& error){return {false,0,error.error};}}
// 缺域、孤立marker、重复marker及未知版本都不折成空成功，避免冷恢复抹掉已发生决定。
Status validate_task_offer_response_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const TaskOfferResponseState* state){
    const StableId* version=nullptr;for(const auto& module:modules)if(module.first=="task_offer_responses"){if(version)return common_failure("DuplicateTaskOfferResponseMarker");version=&module.second;}
    if(!enabled){if(version||state)return common_failure("UnexpectedTaskOfferResponseDomain");return common_success();}
    if(!version)return common_failure("MissingTaskOfferResponseMarker");
    if(*version!="1")return common_failure("UnsupportedTaskOfferResponseVersion");
    if(!state)return common_failure("MissingTaskOfferResponseDomain");
    if(state->version!=1)return common_failure("UnsupportedTaskOfferResponseVersion");
    return common_success();
}
// 空Done本身不能证明能力已启用；只拦本域新命令，不扩张Common的历史强一致性要求。
Status validate_task_offer_response_history(const CommonStore& common,bool enabled){
    return validate_task_offer_response_version_history(common,enabled?1:0);
}
Status validate_task_offer_response_version_history(const CommonStore& common,Integer version){
    if(version<0||version>2)return common_failure("UnsupportedTaskOfferResponseVersion");
    const auto denied=[&](const StableId& kind){return (version==0&&kind=="decline_offer")||(version<2&&kind=="decline_runtime_offer");};
    for(const auto& row:common.state().results.rows())if(row.second&&denied(row.second->kind))return common_failure(row.second->kind=="decline_runtime_offer"?"RuntimeOfferResponseRequiresV2":"TaskOfferResponseProfileRequired","results/"+row.first);
    for(const auto& row:common.state().history)if(denied(row.kind))return common_failure(row.kind=="decline_runtime_offer"?"RuntimeOfferResponseRequiresV2":"TaskOfferResponseProfileRequired","history/"+row.result_id);
    return common_success();
}
Status validate_task_offer_response_version_layout(Integer version,const std::vector<std::pair<StableId,StableId>>& modules,const TaskOfferResponseState* state){
    if(version<0||version>2)return common_failure("UnsupportedTaskOfferResponseVersion");
    if(version<2){auto old=validate_task_offer_response_layout(version==1,modules,state);if(!old.ok)return old;if(state&&state->runtime)return common_failure("RuntimeOfferResponseRequiresV2");return common_success();}
    const StableId* marker=nullptr;for(const auto& module:modules)if(module.first=="task_offer_responses"){if(marker)return common_failure("DuplicateTaskOfferResponseMarker");marker=&module.second;}
    if(!marker)return common_failure("MissingTaskOfferResponseMarker");if(*marker!="2")return common_failure("TaskOfferResponseVersionMismatch");
    if(!state)return common_failure("MissingTaskOfferResponseDomain");if(state->version!=2)return common_failure("TaskOfferResponseVersionMismatch");if(!state->runtime)return common_failure("MissingRuntimeOfferResponses");return common_success();
}
}
