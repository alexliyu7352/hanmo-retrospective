// 沿既有原生采购和经济能力准入，不改全局能力描述摘要；显式域负责自己的授权与冷边界。
#include "task_personal_advance_profile.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
Outcome<std::optional<TaskPersonalAdvanceCatalog>> decode_task_personal_advance_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;object(scenario,"scenario");const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)return {true,std::nullopt,{}};
    object(*dynamic,"scenario/dynamic_world");const auto* definition=dynamic->find("task_personal_advances");if(!definition)return {true,std::nullopt,{}};
    if(get_string(*dynamic,"profile","dynamic_world")!="local_society_v1")unsupported("TaskAdvanceNativeProfileRequired","dynamic_world/profile");
    for(const auto* capability:{"native_dynamic_world","tasks.procurement","economy","multi_actor"})if(std::find(capabilities.begin(),capabilities.end(),capability)==capabilities.end())unsupported("MissingTaskAdvanceCapability",capability);
    auto decoded=decode_task_personal_advance_catalog(*definition);if(!decoded.ok)return {false,{},decoded.error};
    const auto selected=strings(field(field(scenario,"enabled","scenario"),"task_templates","enabled"),"enabled/task_templates");
    for(const auto& row:decoded.value.policies.rows())if(std::find(selected.begin(),selected.end(),row.second.task_template_id)==selected.end())fail("TaskAdvanceTemplateNotEnabled",row.first);
    return {true,std::move(decoded.value),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_task_personal_advance_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const TaskPersonalAdvanceState* state){
    const StableId* marker=nullptr;for(const auto& row:modules)if(row.first=="task_personal_advances"){if(marker)return common_failure("DuplicateTaskAdvanceMarker");marker=&row.second;}
    if(!enabled){if(marker||state)return common_failure("UnexpectedTaskAdvanceDomain");return common_success();}
    if(!marker)return common_failure("MissingTaskAdvanceMarker");if(*marker!="1")return common_failure("UnsupportedTaskAdvanceVersion");
    if(!state)return common_failure("MissingTaskAdvanceDomain");if(state->version!=1)return common_failure("UnsupportedTaskAdvanceVersion");return common_success();
}
Status validate_task_personal_advance_history(const CommonStore& common,bool enabled){
    if(enabled)return common_success();
    for(const auto& row:common.state().history)if(row.kind=="task_personal_advance")return common_failure("TaskAdvanceProfileRequired","history/"+row.result_id);
    for(const auto& row:common.state().results.rows())if(row.second&&row.second->kind=="task_personal_advance")return common_failure("TaskAdvanceProfileRequired","results/"+row.first);
    for(const auto& row:common.state().facts.rows())if(row.second.kind==task_advance_fact_kind||row.second.kind==task_advance_repayment_fact_kind)return common_failure("TaskAdvanceProfileRequired","facts/"+row.first);
    return common_success();
}
}
