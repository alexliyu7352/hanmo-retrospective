#include "resident/kernel/catalog_section_observation.hpp"
// 原始sender与真实Known provider声明逐项相同；重复/公约/其它委托不获得离岗销约口。
#include "task_personal_cancellation_catalog.hpp"
#include <set>
namespace hanmo::resident {namespace {
bool contains(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
void unique(const BoundaryValue& value,const StableId& path){std::set<StableId> seen;for(const auto& field:boundary::object(value,path))if(!seen.insert(field.first).second)boundary::fail("DuplicateTaskPersonalCancellationField",path+"/"+field.first);}
}
const TaskPersonalCancellationPolicy* task_personal_cancellation_policy(const TaskPersonalCancellationCatalog& c,const StableId& offer){const TaskPersonalCancellationPolicy* found=nullptr;for(const auto& p:c.policies)if(p.offer_id==offer){if(found)return nullptr;found=&p;}return found;}
Status validate_task_personal_cancellation_catalog(const TaskPersonalCancellationCatalog& c,const TaskCatalog& tasks,const TaskPersonalMessageCatalog& deliveries,const KnownPersonalDialogueCatalog& personal,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 121, "TaskPersonalCancellationCatalog", "validate_task_personal_cancellation_catalog", &c);

 auto original=validate_task_personal_message_catalog(deliveries,tasks,personal,interaction);if(!original.ok)return original;
 if(c.version!=1||c.policies.empty()||c.policies.size()>4096||personal.version!=1||!interaction.common)return common_failure("InvalidTaskPersonalCancellationCatalog");std::set<StableId> ids,offers;
 for(const auto& p:c.policies){const auto* task=tasks.offers.find(p.offer_id);if(p.id.empty()||p.id.size()>256||!ids.insert(p.id).second||!offers.insert(p.offer_id).second||!task_personal_message_policy(deliveries,p.offer_id)||!task||task->template_id!="message"||task->issuer.kind!=Owner::Kind::actor||task->issuer.extra||!contains(interaction.common->actor_ids,task->issuer.id)||!contains(interaction.common->actor_ids,task->goal.receiver_actor)||task->issuer.id==task->goal.receiver_actor||task->deadline_ticks<1||!interaction.common->items.find(task->goal.message_id))return common_failure("TaskPersonalCancellationOriginalPrivateTaskRequired",p.id);
  const KnownPersonalDialogueTemplate* matched=nullptr;for(const auto& t:personal.templates)if(t.id==p.personal_template_id){if(matched)return common_failure("AmbiguousTaskPersonalCancellationTemplate",p.id);matched=&t;}
  if(!matched||matched->provider.kind!="installed_actor"||matched->provider.source_id!=task->issuer.id)return common_failure("TaskPersonalCancellationOriginalSenderRequired",p.id);
  const DialogueDefinition* dialogue=nullptr;for(const auto& d:interaction.dialogues)if(d.id==matched->dialogue_id){if(dialogue)return common_failure("AmbiguousTaskPersonalCancellationDialogue",p.id);dialogue=&d;}
  const DialogueNode* node=nullptr;if(dialogue)for(const auto& n:dialogue->nodes)if(n.id==p.sender_say_node_id){if(node)return common_failure("AmbiguousTaskPersonalCancellationStatement",p.id);node=&n;}
  if(!dialogue||!dialogue->context_role.empty()||!node||node->kind!="say"||node->command||node->speaker!="$provider")return common_failure("TaskPersonalCancellationSenderStatementRequired",p.id);
 }
 return common_success();
}
Outcome<TaskPersonalCancellationCatalog> decode_task_personal_cancellation_catalog(const BoundaryValue& value,const TaskCatalog& tasks,const TaskPersonalMessageCatalog& deliveries,const KnownPersonalDialogueCatalog& personal,const InteractionCatalog& interaction){try{using namespace boundary;unique(value,"task_personal_message_cancellations");fields(value,{"version","profile","policies"},{},"task_personal_message_cancellations");if(get_int(value,"version","task_personal_message_cancellations")!=1||get_string(value,"profile","task_personal_message_cancellations")!=task_personal_cancellation_profile)unsupported("UnsupportedTaskPersonalCancellationProfile","task_personal_message_cancellations");TaskPersonalCancellationCatalog c;for(const auto& row:array(field(value,"policies","task_personal_message_cancellations"),"task_personal_message_cancellations/policies")){unique(row,"task_personal_cancellation_policy");fields(row,{"id","offer_id","personal_template_id","sender_say_node_id"},{},"task_personal_cancellation_policy");c.policies.push_back({get_string(row,"id","policy"),get_string(row,"offer_id","policy"),get_string(row,"personal_template_id","policy"),get_string(row,"sender_say_node_id","policy")});}auto valid=validate_task_personal_cancellation_catalog(c,tasks,deliveries,personal,interaction);return valid.ok?Outcome<TaskPersonalCancellationCatalog>{true,std::move(c),{}}:Outcome<TaskPersonalCancellationCatalog>{false,{},valid.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_task_personal_cancellation_catalog(const TaskPersonalCancellationCatalog& c){using V=BoundaryValue;V::Array rows;for(const auto& p:c.policies)rows.push_back(boundary::obj({{"id",V{p.id}},{"offer_id",V{p.offer_id}},{"personal_template_id",V{p.personal_template_id}},{"sender_say_node_id",V{p.sender_say_node_id}}}));return boundary::obj({{"version",V{c.version}},{"profile",V{StableId{task_personal_cancellation_profile}}},{"policies",V{std::move(rows)}}});}
}
