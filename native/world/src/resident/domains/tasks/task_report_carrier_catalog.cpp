#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_report_carrier_catalog.hpp"
#include "task_dynamic_projection.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
bool actor(const CommonCatalog&c,const StableId&id){return !id.empty()&&std::find(c.actor_ids.begin(),c.actor_ids.end(),id)!=c.actor_ids.end();}
Status identities(const std::vector<StableId>&ids,const CommonCatalog&c,const StableId&carrier,const StableId&offer){
 if(ids.empty())return common_failure("CarrierOfferActorScopeRequired",offer);
 for(std::size_t i=0;i<ids.size();++i){if(!actor(c,ids[i])||ids[i]==carrier)return common_failure("CarrierOfferActualActorRequired",offer);for(std::size_t j=0;j<i;++j)if(ids[i]==ids[j])return common_failure("DuplicateCarrierOfferActor",offer);}return common_success();
}
template<class A>Status record(A&a,TaskReportCarrierOfferDefinition&v){
 a.field("id",v.id);a.field("rule_version",v.rule_version);a.field("carrier_actor_id",v.carrier_actor_id);a.field("role_id",v.role_id);a.field("dialogue_id",v.dialogue_id);a.field("accept_node_id",v.accept_node_id);a.field("destination_site_id",v.destination_site_id);
 dynamic_codec::strings(a,"allowed_sender_actor_ids",v.allowed_sender_actor_ids);dynamic_codec::strings(a,"allowed_recipient_actor_ids",v.allowed_recipient_actor_ids);
 return a.sequence("allowed_report_categories",v.allowed_report_categories,[](auto&c,auto&category){StableId name;if(!c.reading()){const auto*n=propagation_category_name(category);if(!n)return c.reject("UnknownCarrierReportCategory");name=n;}auto s=c.field("value",name);if(!s.ok)return s;if(c.reading()){auto read=read_propagation_category(name);if(!read.ok)return c.reject("UnknownCarrierReportCategory");category=read.value;}return c.status();});
}
template<class A>Status fields(A&a,TaskReportCarrierOfferCatalog&v){a.field("version",v.version);if(v.version!=1)return a.reject("UnsupportedTaskReportCarrierCatalog");return dynamic_codec::table(a,"offers",v.offers,[](auto&c,auto&v){return record(c,v);});}
}
SourceRef task_report_carrier_offer_source(const TaskReportCarrierOfferDefinition&v){return {"tasks_catalog","report_carrier_offer",v.id};}
Outcome<const TaskReportCarrierOfferDefinition*> read_task_report_carrier_offer(const TaskReportCarrierOfferCatalog&c,const SourceRef&s){
 const auto*p=c.offers.find(s.id);if(c.version!=1||s.domain!="tasks_catalog"||s.kind!="report_carrier_offer"||!p||p->id!=s.id)return {false,nullptr,common_failure("TaskReportCarrierAuthorSourceMissing",s.id).error};return {true,p,{}};
}
Status validate_task_report_carrier_offer_catalog(const TaskReportCarrierOfferCatalog&c,const CommonCatalog&common,const InteractionCatalog&interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 127, "TaskReportCarrierOfferCatalog", "validate_task_report_carrier_offer_catalog", &c);

 if(c.version!=1||c.identity.content_id.empty()||c.identity.content_version.empty()||c.identity.content_digest.empty()||c.identity.ruleset_id.empty())return common_failure("TaskReportCarrierLockedCatalogRequired");
 for(const auto&row:c.offers.rows()){
  const auto&p=row.second;const auto*role=common.roles.find(p.role_id);const auto*place=common.places.find(p.destination_site_id);
  if(row.first!=p.id||p.id.empty()||p.rule_version.empty()||!actor(common,p.carrier_actor_id)||!role||!place||place->settlement_id.empty())return common_failure("InvalidTaskReportCarrierOffer",row.first);
  auto valid=identities(p.allowed_sender_actor_ids,common,p.carrier_actor_id,p.id);if(!valid.ok)return valid;valid=identities(p.allowed_recipient_actor_ids,common,p.carrier_actor_id,p.id);if(!valid.ok)return valid;
  const DialogueDefinition*dialogue=nullptr;for(const auto&d:interaction.dialogues)if(d.id==p.dialogue_id){if(dialogue)return common_failure("AmbiguousCarrierOfferDialogue",p.id);dialogue=&d;}
  if(!dialogue||dialogue->context_role!=p.role_id)return common_failure("CarrierOfferDialogueScopeRequired",p.id);
  const DialogueNode*node=nullptr;for(const auto&n:dialogue->nodes)if(n.id==p.accept_node_id){if(node)return common_failure("AmbiguousCarrierOfferNode",p.id);node=&n;}
  if(!node||node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=p.carrier_actor_id))return common_failure("CarrierOfferProviderDeclarationRequired",p.id);
  if(p.allowed_report_categories.empty()||p.allowed_report_categories.size()>8)return common_failure("CarrierOfferReportScopeRequired",p.id);
  for(std::size_t i=0;i<p.allowed_report_categories.size();++i){if(!propagation_category_name(p.allowed_report_categories[i]))return common_failure("UnknownCarrierReportCategory",p.id);for(std::size_t j=0;j<i;++j)if(p.allowed_report_categories[j]==p.allowed_report_categories[i])return common_failure("DuplicateCarrierReportCategory",p.id);}
 }
 return common_success();
}
Outcome<TaskReportCarrierOfferCatalog> decode_task_report_carrier_offer_catalog(const BoundaryValue&value,const CatalogIdentity&identity,const CommonCatalog&common,const InteractionCatalog&interaction){
 TaskReportCarrierOfferCatalog out;out.identity=identity;dynamic_codec::JsonArchive archive(value);auto valid=fields(archive,out);if(valid.ok)valid=archive.finish();if(valid.ok)valid=validate_task_report_carrier_offer_catalog(out,common,interaction);return valid.ok?Outcome<TaskReportCarrierOfferCatalog>{true,std::move(out),{}}:Outcome<TaskReportCarrierOfferCatalog>{false,{},valid.error};
}
Outcome<BoundaryValue> encode_task_report_carrier_offer_catalog(const TaskReportCarrierOfferCatalog&value){return dynamic_projection::encode(value,[](auto&a,auto&v){return fields(a,v);});}
Status emit_task_report_carrier_offer_catalog(ProjectionWriter&w,std::string_view key,const TaskReportCarrierOfferCatalog&value){return dynamic_projection::emit(w,key,value,[](auto&a,auto&v){return fields(a,v);});}
}
