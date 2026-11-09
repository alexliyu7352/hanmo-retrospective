#include "resident/kernel/catalog_section_observation.hpp"
// 仅真实原私人岗位明确回答指定人物；分享只限其本人可证亲见，拒答不扫描他人知识。
#include "contact_inquiry.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <set>
namespace hanmo::resident {
const ContactInquiryPolicy* contact_inquiry_policy(const ContactInquiryCatalog& catalog,const StableId& id){for(const auto& p:catalog.policies)if(p.id==id)return &p;return nullptr;}
Status validate_contact_inquiry_catalog(const ContactInquiryCatalog& catalog,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 52, "ContactInquiryCatalog", "validate_contact_inquiry_catalog", &catalog);

    if((catalog.version!=1&&catalog.version!=2)||!interaction.common||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidContactInquiryCatalog");
    std::set<StableId> ids;
    for(const auto& p:catalog.policies){
        const auto* role=interaction.common->roles.find(p.role_id);
        if(p.id.empty()||p.id.size()>256||!ids.insert(p.id).second||!role||role->organization_id||role->initial_actor!=p.provider_actor_id||role->dialogue_id!=p.dialogue_id||!interaction.common->places.find(role->place_id)||std::find(interaction.common->actor_ids.begin(),interaction.common->actor_ids.end(),p.provider_actor_id)==interaction.common->actor_ids.end()||p.subject_actor_id.empty()||p.subject_actor_id==p.provider_actor_id||std::find(interaction.common->actor_ids.begin(),interaction.common->actor_ids.end(),p.subject_actor_id)==interaction.common->actor_ids.end()||(p.disclosure!="share_last_seen"&&p.disclosure!="decline")||p.duration_ticks!=1)return common_failure("InvalidContactInquiryPolicy",p.id);
        const DialogueDefinition* dialogue=nullptr;for(const auto& d:interaction.dialogues)if(d.id==p.dialogue_id){if(dialogue)return common_failure("AmbiguousContactInquiryDialogue",p.id);dialogue=&d;}
        if(!dialogue||dialogue->context_role!=p.role_id)return common_failure("ContactInquiryPrivateRoleRequired",p.id);
        std::set<StableId> nodes;
        for(const auto& id:{p.node_id}){
            if(id.empty()||!nodes.insert(id).second)return common_failure("InvalidContactInquiryNode",p.id);
            const DialogueNode* node=nullptr;for(const auto& n:dialogue->nodes)if(n.id==id){if(node)return common_failure("AmbiguousContactInquiryNode",p.id);node=&n;}
            if(!node||node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=p.provider_actor_id)||node->command)return common_failure("ContactInquiryProviderDeclarationRequired",p.id);
        }
    }
    return common_success();
}
Outcome<ContactInquiryCatalog> decode_contact_inquiry_catalog(const BoundaryValue& value,const InteractionCatalog& interaction){
    try{using namespace boundary;const auto unique=[](const BoundaryValue& v,const StableId& path){std::set<StableId> names;for(const auto& row:object(v,path))if(!names.insert(row.first).second)fail("DuplicateContactInquiryField",path+"/"+row.first);};
        unique(value,"contact_inquiries");fields(value,{"version","policies"},{},"contact_inquiries");ContactInquiryCatalog out;out.version=get_int(value,"version","contact_inquiries");
        const auto& rows=array(field(value,"policies","contact_inquiries"),"policies");if(rows.size()>4096)fail("ContactInquiryPolicyLimit","policies");
        for(const auto& row:rows){unique(row,"policy");fields(row,{"id","provider_actor_id","role_id","dialogue_id","node_id","subject_actor_id","disclosure","duration_ticks"},{},"policy");out.policies.push_back({get_string(row,"id","policy"),get_string(row,"provider_actor_id","policy"),get_string(row,"subject_actor_id","policy"),get_string(row,"role_id","policy"),get_string(row,"dialogue_id","policy"),get_string(row,"node_id","policy"),get_string(row,"disclosure","policy"),get_int(row,"duration_ticks","policy")});}
        auto valid=validate_contact_inquiry_catalog(out,interaction);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Status emit_contact_inquiry_catalog_fields(ProjectionWriter&w,const ContactInquiryCatalog&catalog){if(catalog.version!=1&&catalog.version!=2)return w.reject("UnsupportedContactInquiryVersion");w.field("version",catalog.version);return w.sequence("policies",catalog.policies,[&](auto&out,const auto&p){return out.object("",[&](auto&v){v.field("id",p.id);v.field("provider_actor_id",p.provider_actor_id);v.field("role_id",p.role_id);v.field("dialogue_id",p.dialogue_id);v.field("node_id",p.node_id);v.field("subject_actor_id",p.subject_actor_id);v.field("disclosure",p.disclosure);return v.field("duration_ticks",p.duration_ticks);});});}
BoundaryValue encode_contact_inquiry_catalog(const ContactInquiryCatalog&catalog){BoundaryValue out;BoundaryProjectionWriter w(out);auto r=w.object("",[&](auto&v){return emit_contact_inquiry_catalog_fields(v,catalog);});if(r.ok)r=w.finish();if(!r.ok){boundary::DecodeFailure failure(r.error.original.code,r.error.original.path);failure.error=r.error;throw failure;}return out;}
Outcome<ContactInquiryState> make_initial_contact_inquiry_state(const ContactInquiryCatalog&catalog,const InteractionCatalog&definitions){auto v=validate_contact_inquiry_catalog(catalog,definitions);if(!v.ok)return {false,{},v.error};ContactInquiryState state;state.version=catalog.version;return {true,std::move(state),{}};}

}
