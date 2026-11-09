#include "resident/kernel/catalog_section_observation.hpp"
// 作者说话节点表达委托人自己的有限延期许可；不从台词文本猜同意，不支持公约代签。
#include "task_extension.hpp"
#include <set>
namespace hanmo::resident {
Status validate_task_extension_catalog(const TaskExtensionCatalog& catalog,const TaskCatalog& tasks,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 117, "TaskExtensionCatalog", "validate_task_extension_catalog", &catalog);

    if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096||!interaction.common)return common_failure("InvalidTaskExtensionCatalog");
    std::set<StableId> ids,offers;
    for(const auto& policy:catalog.policies){
        const auto* task=tasks.offers.find(policy.offer_id);const auto* role=task?interaction.common->roles.find(task->report_role):nullptr;
        if(policy.id.empty()||policy.id.size()>256||!ids.insert(policy.id).second||!offers.insert(policy.offer_id).second||policy.additional_ticks<1||policy.additional_ticks>720||!task||task->issuer.kind!=Owner::Kind::actor||task->issuer.extra||task->issuer.id.empty()||task->deadline_ticks<1||(task->template_id!="procurement"&&task->template_id!="message")||!role||role->organization_id||role->initial_actor!=task->issuer.id||!role->dialogue_id||*role->dialogue_id!=policy.dialogue_id||std::find(role->services.begin(),role->services.end(),"report_offer")==role->services.end())return common_failure("InvalidTaskExtensionPrivateSource",policy.id);
        // 护送失败的同tick战斗历史须另接权威来源；首版不借当前failure字段倒填当时可延期。
        const DialogueDefinition* dialogue=nullptr;for(const auto& row:interaction.dialogues)if(row.id==policy.dialogue_id){if(dialogue)return common_failure("AmbiguousTaskExtensionDialogue",policy.id);dialogue=&row;}
        const DialogueNode* node=nullptr;if(dialogue)for(const auto& row:dialogue->nodes)if(row.id==policy.node_id){if(node)return common_failure("AmbiguousTaskExtensionNode",policy.id);node=&row;}
        if(!dialogue||dialogue->context_role!=role->id||!node||node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=task->issuer.id)||node->command)return common_failure("TaskExtensionProviderDeclarationRequired",policy.id);
    }
    return common_success();
}
Outcome<TaskExtensionCatalog> decode_task_extension_catalog(const BoundaryValue& value,const TaskCatalog& tasks,const InteractionCatalog& interaction){
    try{using namespace boundary;fields(value,{"version","profile","policies"},{},"task_extensions");if(get_int(value,"version","task_extensions")!=1||get_string(value,"profile","task_extensions")!=task_extension_profile)unsupported("UnsupportedTaskExtensionProfile","task_extensions");TaskExtensionCatalog out;std::set<StableId> keys;for(const auto& pair:object(value,"task_extensions"))if(!keys.insert(pair.first).second)fail("DuplicateTaskExtensionField",pair.first);
        for(const auto& entry:array(field(value,"policies","task_extensions"),"task_extensions/policies")){fields(entry,{"id","offer_id","dialogue_id","node_id","additional_ticks"},{},"task_extension_policy");keys.clear();for(const auto& pair:object(entry,"task_extension_policy"))if(!keys.insert(pair.first).second)fail("DuplicateTaskExtensionField",pair.first);out.policies.push_back({get_string(entry,"id","policy"),get_string(entry,"offer_id","policy"),get_string(entry,"dialogue_id","policy"),get_string(entry,"node_id","policy"),get_int(entry,"additional_ticks","policy")});}
        auto valid=validate_task_extension_catalog(out,tasks,interaction);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
BoundaryValue encode_task_extension_catalog(const TaskExtensionCatalog& catalog){using V=BoundaryValue;V::Array rows;for(const auto& p:catalog.policies)rows.push_back(boundary::obj({{"id",V{p.id}},{"offer_id",V{p.offer_id}},{"dialogue_id",V{p.dialogue_id}},{"node_id",V{p.node_id}},{"additional_ticks",V{p.additional_ticks}}}));return boundary::obj({{"version",V{catalog.version}},{"profile",V{StableId{task_extension_profile}}},{"policies",V{std::move(rows)}}});}
Outcome<bool> decode_task_extension_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){
    try{using namespace boundary;const auto* block=scenario.find("task_extensions");if(!block)return {true,false,{}};std::set<StableId> keys;for(const auto& row:object(*block,"task_extensions"))if(!keys.insert(row.first).second)fail("DuplicateTaskExtensionField",row.first);if(std::find(capabilities.begin(),capabilities.end(),task_extension_capability)==capabilities.end())unsupported("TaskExtensionCapabilityRequired","task_extensions");fields(*block,{"version","profile","policies"},{},"task_extensions");if(get_int(*block,"version","task_extensions")!=1||get_string(*block,"profile","task_extensions")!=task_extension_profile)unsupported("UnsupportedTaskExtensionProfile","task_extensions");const auto* dynamic=scenario.find("dynamic_world");if(!dynamic||get_string(*dynamic,"profile","dynamic_world")!="local_society_v1")unsupported("TaskExtensionNativeProfileRequired","task_extensions");return {true,true,{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Status validate_task_extension_layout(const CommonStore& common,const std::vector<std::pair<StableId,StableId>>& modules,const TaskExtensionCatalog* catalog,const TaskExtensionState* state){
    Integer count=0;for(const auto& module:modules)if(module.first==task_extension_module){++count;if(module.second!="1")return common_failure("UnsupportedTaskExtensionVersion");}if(bool(catalog)!=bool(state)||count!=(catalog?1:0)||(catalog&&(catalog->version!=1||state->version!=1)))return common_failure("TaskExtensionProfileMismatch");if(state)return common_success();for(const auto& row:common.state().facts.rows())if(row.second.domain==task_extension_module||row.second.kind==task_extension_fact_kind)return common_failure("TaskExtensionProfileRequired",row.first);return common_success();
}
}
