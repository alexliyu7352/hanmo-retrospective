#include "resident/kernel/catalog_section_observation.hpp"
// v1仅固定三名真实人物与同一接待岗位，换人、道路偶遇、任意文字载荷均不扩大旧授权。
#include "visit_message_catalog.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
void unique(const BoundaryValue& v,const StableId& path){std::set<StableId> seen;for(const auto& row:boundary::object(v,path))if(!seen.insert(row.first).second)boundary::fail("DuplicateVisitMessageField",path+"/"+row.first);}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
}
Status validate_visit_message_catalog(const VisitMessageCatalog& catalog,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 61, "VisitMessageCatalog", "validate_visit_message_catalog", &catalog);

    if((catalog.version<1||catalog.version>3))return common_failure("UnsupportedVisitMessageVersion");
    if(!interaction.common||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidVisitMessageCatalog");
    std::set<StableId> ids;std::set<std::tuple<StableId,StableId,StableId>> pairs;
    for(const auto& policy:catalog.policies){
        if(catalog.version<3&&policy.report_custody_included_in_contact)return common_failure("VisitReportCustodyRequiresVersion3",policy.id);
        if(policy.id.empty()||!ids.insert(policy.id).second||policy.sender_actor_id==policy.recipient_actor_id||policy.sender_actor_id==policy.custodian_actor_id||policy.recipient_actor_id==policy.custodian_actor_id
            ||!pairs.emplace(policy.sender_actor_id,policy.recipient_actor_id,policy.custodian_actor_id).second)return common_failure("InvalidVisitMessageParties",policy.id);
        for(const auto& actor:{policy.sender_actor_id,policy.recipient_actor_id,policy.custodian_actor_id})if(actor.empty()||!has(interaction.common->actor_ids,actor))return common_failure("UnknownVisitMessageActor",policy.id);
        const auto* role=interaction.common->roles.find(policy.role_id);
        if(!role||role->initial_actor!=policy.custodian_actor_id||!interaction.common->places.find(role->place_id)||role->dialogue_id!=policy.dialogue_id)return common_failure("InvalidVisitMessageCustodian",policy.id);
        const DialogueDefinition* dialogue=nullptr;
        for(const auto& row:interaction.dialogues)if(row.id==policy.dialogue_id){if(dialogue)return common_failure("InvalidVisitMessageDialogue",policy.id);dialogue=&row;}
        if(!dialogue||dialogue->context_role!=policy.role_id)return common_failure("InvalidVisitMessageDialogue",policy.id);
        std::set<StableId> nodes;
        for(const auto& id:{policy.deposit_node_id,policy.receive_node_id,policy.withdraw_node_id}){
            if(id.empty()||!nodes.insert(id).second)return common_failure("InvalidVisitMessageNode",policy.id);const DialogueNode* node=nullptr;
            for(const auto& row:dialogue->nodes)if(row.id==id){if(node)return common_failure("InvalidVisitMessageNode",policy.id);node=&row;}
            if(!node||node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=policy.custodian_actor_id))return common_failure("VisitMessageProviderDeclarationRequired",policy.id);
        }
    }
    return common_success();
}
Outcome<VisitMessageState> make_initial_visit_message_state(const VisitMessageCatalog& catalog,const InteractionCatalog& definitions){auto valid=validate_visit_message_catalog(catalog,definitions);if(!valid.ok)return {false,{},valid.error};VisitMessageState state;state.version=catalog.version;return {true,std::move(state),{}};}
Outcome<VisitMessageCatalog> decode_visit_message_catalog(const BoundaryValue& value,const InteractionCatalog& interaction){try{
    using namespace boundary;unique(value,"visit_messages");fields(value,{"version","policies"},{},"visit_messages");VisitMessageCatalog result;result.version=get_int(value,"version","visit_messages");
    if((result.version<1||result.version>3))unsupported("UnsupportedVisitMessageVersion","visit_messages");const auto& rows=array(field(value,"policies","visit_messages"),"policies");if(rows.size()>4096)fail("VisitMessagePolicyLimit","policies");
    for(const auto& row:rows){unique(row,"policy");if(result.version<3)fields(row,{"id","sender_actor_id","recipient_actor_id","custodian_actor_id","role_id","dialogue_id","deposit_node_id","receive_node_id","withdraw_node_id"},{},"policy");else fields(row,{"id","sender_actor_id","recipient_actor_id","custodian_actor_id","role_id","dialogue_id","deposit_node_id","receive_node_id","withdraw_node_id","report_custody_included_in_contact"},{},"policy");
        result.policies.push_back({get_string(row,"id","policy"),get_string(row,"sender_actor_id","policy"),get_string(row,"recipient_actor_id","policy"),get_string(row,"custodian_actor_id","policy"),get_string(row,"role_id","policy"),get_string(row,"dialogue_id","policy"),get_string(row,"deposit_node_id","policy"),get_string(row,"receive_node_id","policy"),get_string(row,"withdraw_node_id","policy")});if(result.version==3)result.policies.back().report_custody_included_in_contact=get_bool(row,"report_custody_included_in_contact","policy");}
    auto checked=validate_visit_message_catalog(result,interaction);if(!checked.ok)return {false,{},checked.error};return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status emit_visit_message_catalog_fields(ProjectionWriter&w,const VisitMessageCatalog&catalog){
 if((catalog.version<1||catalog.version>3))return w.reject("UnsupportedVisitMessageVersion");
 w.field("version",catalog.version);return w.sequence("policies",catalog.policies,[&](auto&out,const auto&p){return out.object("",[&](auto&v){v.field("id",p.id);v.field("sender_actor_id",p.sender_actor_id);v.field("recipient_actor_id",p.recipient_actor_id);v.field("custodian_actor_id",p.custodian_actor_id);v.field("role_id",p.role_id);v.field("dialogue_id",p.dialogue_id);v.field("deposit_node_id",p.deposit_node_id);v.field("receive_node_id",p.receive_node_id);v.field("withdraw_node_id",p.withdraw_node_id);if(catalog.version==3)return v.field("report_custody_included_in_contact",p.report_custody_included_in_contact);return p.report_custody_included_in_contact?v.reject("UnselectedVisitReportCustodyPolicy"):v.status();});});
}
BoundaryValue encode_visit_message_catalog(const VisitMessageCatalog&catalog){BoundaryValue out;BoundaryProjectionWriter w(out);auto r=w.object("",[&](auto&v){return emit_visit_message_catalog_fields(v,catalog);});if(r.ok)r=w.finish();if(!r.ok){boundary::DecodeFailure failure(r.error.original.code,r.error.original.path);failure.error=r.error;throw failure;}return out;}
Outcome<Integer> decode_visit_message_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;unique(scenario,"scenario");const auto* profile=scenario.find("visit_message_profile");const auto* catalog=scenario.find("visit_messages");
    if(!profile){if(catalog)unsupported("VisitMessageProfileRequired","visit_messages");return {true,0,{}};}
    const auto name=string(*profile,"visit_message_profile");const Integer version=name=="witnessed_reception_v1"?1:name=="witnessed_reception_life_v2"?2:name=="witnessed_report_custody_v3"?3:0;if(!version)unsupported("UnsupportedVisitMessageProfile","visit_message_profile");
    if(!catalog)unsupported("VisitMessageCatalogRequired","visit_messages");
    if(!has(capabilities,version==1?"visit_messages_v1":version==2?"visit_messages_v2":"visit_messages_v3"))unsupported("VisitMessageCapabilityRequired","visit_messages");if(get_int(*catalog,"version","visit_messages")!=version)unsupported("VisitMessageProfileMismatch","visit_messages");
    for(const auto* cap:{"dialogue","multi_actor","native_dynamic_world"})if(!has(capabilities,cap))unsupported("VisitMessageCapabilityRequired",cap);
    const auto* dynamic=scenario.find("dynamic_world");if(!dynamic||get_string(*dynamic,"profile","dynamic_world")!="local_society_v1")unsupported("VisitMessageNativeProfileRequired","dynamic_world");return {true,version,{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Outcome<bool> decode_visit_message_profile(const BoundaryValue&scenario,const std::vector<StableId>&capabilities){auto r=decode_visit_message_profile_version(scenario,capabilities);return r.ok?Outcome<bool>{true,r.value!=0,{}}:Outcome<bool>{false,{},r.error};}
Status validate_visit_message_layout(const CommonStore& common,const std::vector<std::pair<StableId,StableId>>& modules,const VisitMessageCatalog* catalog,const VisitMessageState* state){
    Integer count=0;for(const auto& row:modules)if(row.first=="visit_messages"){++count;if(row.second!="1"&&row.second!="2"&&row.second!="3")return common_failure("UnsupportedVisitMessageVersion");}
    if(count!=(catalog?1:0)||bool(catalog)!=bool(state)||(catalog&&(catalog->version<1||catalog->version>3))||(state&&state->version!=catalog->version))return common_failure("VisitMessageProfileMismatch");
    if(catalog)for(const auto&row:modules)if(row.first=="visit_messages"&&row.second!=std::to_string(catalog->version))return common_failure("VisitMessageProfileMismatch");
    if(!state)for(const auto& row:common.state().facts.rows())if(row.second.domain=="visit_messages"||row.second.kind==visit_message_posted_kind||row.second.kind==visit_message_received_kind||row.second.kind==visit_message_withdrawn_kind)return common_failure("VisitMessageProfileRequired",row.first);
    return common_success();
}
}
