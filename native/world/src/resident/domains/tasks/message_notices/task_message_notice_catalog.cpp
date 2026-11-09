#include "resident/kernel/catalog_section_observation.hpp"
// 原私信主题需显式绑定；纯读证明存在不等于作者授权发信，未知字段或依赖不猜补。
#include "task_message_notice_catalog.hpp"
#include <set>
namespace hanmo::resident {
namespace {
void unique(const BoundaryValue& value,const StableId& path){std::set<StableId> keys;for(const auto& row:boundary::object(value,path))if(!keys.insert(row.first).second)boundary::fail("DuplicateTaskMessageNoticeField",path+"/"+row.first);}
bool contains(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
}
Outcome<std::optional<TaskMessageNoticeCatalog>> decode_task_message_notice_catalog(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;unique(scenario,"scenario");const auto* value=scenario.find("task_message_notices");if(!value)return {true,std::nullopt,{}};unique(*value,"task_message_notices");fields(*value,{"version","profile","bindings"},{},"task_message_notices");
    if(get_int(*value,"version","task_message_notices")!=1||get_string(*value,"profile","task_message_notices")!="witnessed_death_letters_v1")unsupported("UnsupportedTaskMessageNoticeProfile","task_message_notices");
    for(const auto* capability:{"task_death_notices_v1","task_message_handoffs_v1","family_life_v1","tasks.message","native_dynamic_world","dialogue","multi_actor"})if(!contains(capabilities,capability))unsupported("TaskMessageNoticeCapabilityRequired",capability);
    const auto* handoff=scenario.find("task_message_handoff_profile");if(!handoff||(string(*handoff,"task_message_handoff_profile")!="observed_letter_delivery_v1"&&string(*handoff,"task_message_handoff_profile")!="observed_letter_delivery_and_report_v2"))unsupported("TaskMessageNoticeHandoffRequired","task_message_handoff_profile");
    const auto* life=scenario.find("family_life");if(!life||get_string(*life,"profile","family_life")!="verified_military_deaths_v1")unsupported("TaskMessageNoticeDeathSourceRequired","family_life");const auto* dynamic=scenario.find("dynamic_world");if(!dynamic||get_string(*dynamic,"profile","dynamic_world")!="local_society_v1")unsupported("TaskMessageNoticeNativeProfileRequired","dynamic_world");
    const auto& rows=array(field(*value,"bindings","task_message_notices"),"task_message_notices/bindings");if(rows.empty()||rows.size()>4096)fail("InvalidTaskMessageNoticeBindings","task_message_notices/bindings");TaskMessageNoticeCatalog result;
    for(const auto& row:rows){unique(row,"binding");fields(row,{"offer_id","decedent_actor_id"},{},"binding");TaskDeathNoticeBindingPolicy policy{get_string(row,"offer_id","binding"),get_string(row,"decedent_actor_id","binding")};if(policy.offer_id.empty()||policy.decedent_actor_id.empty())fail("InvalidTaskMessageNoticeBinding","binding");const auto id=policy.offer_id;if(!result.bindings.insert(id,std::move(policy)))fail("DuplicateTaskMessageNoticeOffer",id);}return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_task_message_notice_catalog(const TaskMessageNoticeCatalog& catalog,const TaskCatalog& tasks,const CommonCatalog& common){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 118, "TaskMessageNoticeCatalog", "validate_task_message_notice_catalog", &catalog);

    if(catalog.version!=1||catalog.bindings.empty())return common_failure("UnsupportedTaskMessageNoticeVersion");for(const auto& row:catalog.bindings.rows()){const auto& policy=row.second;const auto* offer=tasks.offers.find(policy.offer_id);if(row.first!=policy.offer_id||!offer||offer->template_id!="message"||offer->issuer.kind!=Owner::Kind::actor||offer->issuer.id==offer->goal.receiver_actor||policy.decedent_actor_id==offer->issuer.id||policy.decedent_actor_id==offer->goal.receiver_actor||!contains(common.actor_ids,policy.decedent_actor_id)||!contains(common.actor_ids,offer->issuer.id)||!contains(common.actor_ids,offer->goal.receiver_actor)||!common.items.find(offer->goal.message_id))return common_failure("InvalidTaskMessageNoticeOfferSource",row.first);}return common_success();
}
}
