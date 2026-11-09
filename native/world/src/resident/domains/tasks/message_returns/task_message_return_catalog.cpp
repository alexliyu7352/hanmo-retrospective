#include "resident/kernel/catalog_section_observation.hpp"
// 回书条款只绑定原收件人的本人声明；目录验证不生产信件、Task或任何同意事实。
#include "task_message_return_catalog.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff_profile.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 重复字段必须先拒绝，不能借解析器取首值把两套作者条款藏在同一对象中。
void unique_fields(const BoundaryValue& v,const StableId& path){std::set<StableId> keys;for(const auto& pair:boundary::object(v,path))if(!keys.insert(pair.first).second)boundary::fail("DuplicateTaskMessageReturnField",path+"/"+pair.first);}
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
}
const TaskMessageReturnPolicy* task_message_return_policy(const TaskMessageReturnCatalog& catalog,const StableId& offer){const TaskMessageReturnPolicy* found=nullptr;for(const auto& p:catalog.policies)if(p.offer_id==offer){if(found)return nullptr;found=&p;}return found;}
Status validate_task_message_return_catalog(const TaskMessageReturnCatalog& catalog,const TaskCatalog& tasks,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 119, "TaskMessageReturnCatalog", "validate_task_message_return_catalog", &catalog);

 if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096||!interaction.common)return common_failure("InvalidTaskMessageReturnCatalog");
 const auto& common=*interaction.common;std::set<StableId> ids,offers;
 for(const auto& p:catalog.policies){
  const auto* task=tasks.offers.find(p.offer_id);const auto* role=common.roles.find(p.recipient_role_id);const auto* report=task?common.roles.find(task->report_role):nullptr;
  if(p.id.empty()||p.id.size()>256||!ids.insert(p.id).second||!offers.insert(p.offer_id).second||!task||task->template_id!="message"||task->issuer.kind!=Owner::Kind::actor||task->issuer.extra||!contains(common.actor_ids,task->issuer.id)||!contains(common.actor_ids,task->goal.receiver_actor)||task->issuer.id==task->goal.receiver_actor||task->deadline_ticks<1||p.reply_item_id.empty()||p.reply_item_id==task->goal.message_id||!common.items.find(p.reply_item_id)||!common.items.find(task->goal.message_id))return common_failure("InvalidTaskMessageReturnPrivateTask",p.id);
  if(!role||role->organization_id||role->initial_actor!=task->goal.receiver_actor||!role->dialogue_id||*role->dialogue_id!=p.dialogue_id||!common.places.find(role->place_id)||!contains(role->services,"deliver_message")||!report||report->organization_id||report->initial_actor!=task->issuer.id||!common.places.find(report->place_id)||!contains(report->services,"report_offer")||!report->dialogue_id)return common_failure("TaskMessageReturnOriginalPartiesRequired",p.id);
  bool sender_say=false;for(const auto& talk:interaction.dialogues)if(talk.id==*report->dialogue_id&&talk.context_role==report->id)for(const auto& statement:talk.nodes)if(statement.kind=="say"&&!statement.command&&(statement.speaker=="$provider"||statement.speaker==task->issuer.id))sender_say=true;if(!sender_say)return common_failure("TaskMessageReturnSenderSayRequired",p.id);
  const DialogueDefinition* dialogue=nullptr;for(const auto& row:interaction.dialogues)if(row.id==p.dialogue_id){if(dialogue)return common_failure("AmbiguousTaskMessageReturnDialogue",p.id);dialogue=&row;}
  const DialogueNode* node=nullptr;if(dialogue)for(const auto& row:dialogue->nodes)if(row.id==p.node_id){if(node)return common_failure("AmbiguousTaskMessageReturnNode",p.id);node=&row;}
  if(!dialogue||dialogue->context_role!=p.recipient_role_id||!node||node->kind!="say"||node->command||(node->speaker!="$provider"&&node->speaker!=task->goal.receiver_actor))return common_failure("TaskMessageReturnRecipientDeclarationRequired",p.id);
 }
 return common_success();
}
Outcome<TaskMessageReturnCatalog> decode_task_message_return_catalog(const BoundaryValue& value,const TaskCatalog& tasks,const InteractionCatalog& interaction){try{
 using namespace boundary;unique_fields(value,"task_message_returns");fields(value,{"version","profile","policies"},{},"task_message_returns");if(get_int(value,"version","task_message_returns")!=1||get_string(value,"profile","task_message_returns")!=task_message_return_profile)unsupported("UnsupportedTaskMessageReturnProfile","task_message_returns");TaskMessageReturnCatalog out;
 for(const auto& entry:array(field(value,"policies","task_message_returns"),"task_message_returns/policies")){unique_fields(entry,"task_message_return_policy");fields(entry,{"id","offer_id","reply_item_id","recipient_role_id","dialogue_id","node_id"},{},"task_message_return_policy");out.policies.push_back({get_string(entry,"id","policy"),get_string(entry,"offer_id","policy"),get_string(entry,"reply_item_id","policy"),get_string(entry,"recipient_role_id","policy"),get_string(entry,"dialogue_id","policy"),get_string(entry,"node_id","policy")});}
 auto valid=validate_task_message_return_catalog(out,tasks,interaction);return valid.ok?Outcome<TaskMessageReturnCatalog>{true,std::move(out),{}}:Outcome<TaskMessageReturnCatalog>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_task_message_return_catalog(const TaskMessageReturnCatalog& catalog){using V=BoundaryValue;V::Array rows;for(const auto& p:catalog.policies)rows.push_back(boundary::obj({{"id",V{p.id}},{"offer_id",V{p.offer_id}},{"reply_item_id",V{p.reply_item_id}},{"recipient_role_id",V{p.recipient_role_id}},{"dialogue_id",V{p.dialogue_id}},{"node_id",V{p.node_id}}}));return boundary::obj({{"version",V{catalog.version}},{"profile",V{StableId{task_message_return_profile}}},{"policies",V{std::move(rows)}}});}
Outcome<bool> decode_task_message_return_profile(const BoundaryValue& scenario,const std::vector<StableId>& caps){try{
 using namespace boundary;unique_fields(scenario,"scenario");const auto* block=scenario.find("task_message_returns");if(!block)return {true,false,{}};unique_fields(*block,"task_message_returns");fields(*block,{"version","profile","policies"},{},"task_message_returns");if(get_int(*block,"version","task_message_returns")!=1||get_string(*block,"profile","task_message_returns")!=task_message_return_profile)unsupported("UnsupportedTaskMessageReturnProfile","task_message_returns");
 for(const auto* cap:{task_message_return_capability,"task_static_assets_v1","family_life_v1"})if(!contains(caps,cap))unsupported("TaskMessageReturnCapabilityRequired",cap);
 auto messages=decode_task_message_handoff_version(scenario,caps);if(!messages.ok)return {false,{},messages.error};if(messages.value!=2)unsupported("TaskMessageReturnReportProfileRequired","task_message_returns");auto assets=decode_task_static_asset_profile(scenario,caps);if(!assets.ok)return assets;if(!assets.value||!scenario.find("family_life"))unsupported("TaskMessageReturnOriginalSourcesRequired","task_message_returns");
 return {true,true,{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
