#include "resident/kernel/catalog_section_observation.hpp"
// 首片只约原私人岗位本人，无费用、无代签、无秘密载荷；公共接见须另有明确权源。
#include "personal_appointment.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <set>
namespace hanmo::resident {
const PersonalAppointmentPolicy* personal_appointment_policy(const PersonalAppointmentCatalog& catalog,const StableId& id){for(const auto& p:catalog.policies)if(p.id==id)return &p;return nullptr;}
Status validate_personal_appointment_catalog(const PersonalAppointmentCatalog& catalog,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 49, "PersonalAppointmentCatalog", "validate_personal_appointment_catalog", &catalog);

    if((catalog.version!=1&&catalog.version!=2)||!interaction.common||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPersonalAppointmentCatalog");
    std::set<StableId> ids;
    for(const auto& p:catalog.policies){
        const auto* role=interaction.common->roles.find(p.role_id);
        if(p.id.empty()||p.id.size()>256||!ids.insert(p.id).second||!role||role->organization_id||role->initial_actor!=p.provider_actor_id||role->dialogue_id!=p.dialogue_id||!interaction.common->places.find(role->place_id)||std::find(interaction.common->actor_ids.begin(),interaction.common->actor_ids.end(),p.provider_actor_id)==interaction.common->actor_ids.end()||p.earliest_offset<1||p.latest_offset<p.earliest_offset||p.latest_offset>720||p.duration_ticks!=1)return common_failure("InvalidPersonalAppointmentPolicy",p.id);
        const DialogueDefinition* dialogue=nullptr;for(const auto& d:interaction.dialogues)if(d.id==p.dialogue_id){if(dialogue)return common_failure("AmbiguousPersonalAppointmentDialogue",p.id);dialogue=&d;}
        if(!dialogue||dialogue->context_role!=p.role_id)return common_failure("PersonalAppointmentPrivateRoleRequired",p.id);
        std::set<StableId> nodes;
        for(const auto& id:{p.accept_node_id,p.cancel_node_id,p.meet_node_id}){
            if(id.empty()||!nodes.insert(id).second)return common_failure("InvalidPersonalAppointmentNode",p.id);
            const DialogueNode* node=nullptr;for(const auto& n:dialogue->nodes)if(n.id==id){if(node)return common_failure("AmbiguousPersonalAppointmentNode",p.id);node=&n;}
            if(!node||node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=p.provider_actor_id)||node->command)return common_failure("PersonalAppointmentProviderDeclarationRequired",p.id);
        }
    }
    return common_success();
}
Outcome<PersonalAppointmentCatalog> decode_personal_appointment_catalog(const BoundaryValue& value,const InteractionCatalog& interaction){
    try{using namespace boundary;const auto unique=[](const BoundaryValue& v,const StableId& path){std::set<StableId> names;for(const auto& row:object(v,path))if(!names.insert(row.first).second)fail("DuplicatePersonalAppointmentField",path+"/"+row.first);};
        unique(value,"personal_appointments");fields(value,{"version","policies"},{},"personal_appointments");PersonalAppointmentCatalog out;out.version=get_int(value,"version","personal_appointments");
        const auto& rows=array(field(value,"policies","personal_appointments"),"policies");if(rows.size()>4096)fail("PersonalAppointmentPolicyLimit","policies");
        for(const auto& row:rows){unique(row,"policy");fields(row,{"id","provider_actor_id","role_id","dialogue_id","accept_node_id","cancel_node_id","meet_node_id","earliest_offset","latest_offset","duration_ticks"},{},"policy");out.policies.push_back({get_string(row,"id","policy"),get_string(row,"provider_actor_id","policy"),get_string(row,"role_id","policy"),get_string(row,"dialogue_id","policy"),get_string(row,"accept_node_id","policy"),get_string(row,"cancel_node_id","policy"),get_string(row,"meet_node_id","policy"),get_int(row,"earliest_offset","policy"),get_int(row,"latest_offset","policy"),get_int(row,"duration_ticks","policy")});}
        auto valid=validate_personal_appointment_catalog(out,interaction);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Status emit_personal_appointment_catalog_fields(ProjectionWriter&w,const PersonalAppointmentCatalog&catalog){
 if(catalog.version!=1&&catalog.version!=2)return w.reject("UnsupportedPersonalAppointmentVersion");
 w.field("version",catalog.version);return w.sequence("policies",catalog.policies,[&](auto&out,const auto&p){return out.object("",[&](auto&v){v.field("id",p.id);v.field("provider_actor_id",p.provider_actor_id);v.field("role_id",p.role_id);v.field("dialogue_id",p.dialogue_id);v.field("accept_node_id",p.accept_node_id);v.field("cancel_node_id",p.cancel_node_id);v.field("meet_node_id",p.meet_node_id);v.field("earliest_offset",p.earliest_offset);v.field("latest_offset",p.latest_offset);return v.field("duration_ticks",p.duration_ticks);});});
}
BoundaryValue encode_personal_appointment_catalog(const PersonalAppointmentCatalog&catalog){BoundaryValue out;BoundaryProjectionWriter w(out);auto r=w.object("",[&](auto&v){return emit_personal_appointment_catalog_fields(v,catalog);});if(r.ok)r=w.finish();if(!r.ok){boundary::DecodeFailure failure(r.error.original.code,r.error.original.path);failure.error=r.error;throw failure;}return out;}
Outcome<PersonalAppointmentState> make_initial_personal_appointment_state(const PersonalAppointmentCatalog&catalog,const InteractionCatalog&definitions){auto v=validate_personal_appointment_catalog(catalog,definitions);if(!v.ok)return {false,{},v.error};PersonalAppointmentState state;state.version=catalog.version;return {true,std::move(state),{}};}

}
