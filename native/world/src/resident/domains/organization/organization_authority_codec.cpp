#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
// 复用原组织每个record的字段序，仅新4/5在同一事实尾部增加typed来源并保存唯一现行Grant。
#include "organization_authority_codec.hpp"
#include "organization_service_effects_codec.hpp"
#include "organization_authority_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
using namespace organization_authority_codec_fields;

Status visit_organization_checkpoint_v4(CodecVisitor& c,OrganizationState& state,bool named){if(!c.reading()){auto valid=validate_organization_service_effects(state,0);if(!valid.ok)return valid;}return value(c,"organization",state,named);}
Status emit_organization_state_v4_projection(ProjectionWriter& sink,std::string_view name,const OrganizationState& input,bool named){auto valid=validate_organization_service_effects(input,0);if(!valid.ok)return valid;return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,input,[&](auto& c,auto& value){return record_value(c,value,named);});}
Outcome<BoundaryValue> encode_organization_state_v4(const OrganizationState& input,bool named){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_organization_state_v4_projection(sink,"",input,named);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<OrganizationState> decode_organization_state_v4(const BoundaryValue& input,bool named){ActorFoundationJsonVisitor c(input);auto valid=prepare_checkpoint_default_values<OrganizationState>(c);if(!valid.ok)return {false,{},valid.error};if(!ScopedCheckpointJsonDecode::current())if(auto* owner=ScopedCheckpointBoundaryDecode::current())owner->default_values<OrganizationState>();OrganizationState out;valid=record_value(c,out,named);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<OrganizationCatalog> decode_organization_catalog_v4(const BoundaryValue& input,const CatalogIdentity& identity,bool named){const auto* section=input.find("organization");ActorFoundationJsonVisitor c(section?*section:input);auto valid=prepare_checkpoint_default_values<OrganizationCatalog>(c);if(!valid.ok)return {false,{},valid.error};if(!ScopedCheckpointJsonDecode::current())if(auto* owner=ScopedCheckpointBoundaryDecode::current())owner->default_values<OrganizationCatalog>();OrganizationCatalog out;valid=record_value(c,out,named);if(valid.ok)valid=c.finish();if(valid.ok){out.authority_lifecycle_enabled=true;
 // The supplied immutable identity is not a physical field in this body.
 // Precharge its one fresh copy into the actual decoded candidate owner,
 // not a local lease that dies before the returned Catalog is adopted.

 if(auto* owner=ScopedCheckpointJsonDecode::current())owner->ordinary(std::size_t{0});
 else if(auto* owner=ScopedCheckpointBoundaryDecode::current())owner->ordinary(std::size_t{0});
 out.authority_catalog_identity=CatalogIdentity(identity);}return {valid.ok,std::move(out),valid.error};}
Status visit_organization_runtime_body(CodecVisitor& c,OrganizationState& state,bool named){if(!c.reading()){auto valid=validate_organization_service_effects(state,0);if(!valid.ok)return valid;}return value(c,"organization",state,named,false);}
Status emit_organization_runtime_body_projection(ProjectionWriter& sink,std::string_view name,const OrganizationState& input,bool named){auto valid=validate_organization_service_effects(input,0);if(!valid.ok)return valid;return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,input,[&](auto& c,auto& value){return record_value(c,value,named,false);});}
Outcome<BoundaryValue> encode_organization_runtime_body(const OrganizationState& input,bool named){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_organization_runtime_body_projection(sink,"",input,named);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<OrganizationState> decode_organization_runtime_body(const BoundaryValue& input,bool named){ActorFoundationJsonVisitor c(input);auto valid=prepare_checkpoint_default_values<OrganizationState>(c);if(!valid.ok)return {false,{},valid.error};if(!ScopedCheckpointJsonDecode::current())if(auto* owner=ScopedCheckpointBoundaryDecode::current())owner->default_values<OrganizationState>();OrganizationState out;valid=record_value(c,out,named,false);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}

Outcome<OrganizationCatalog> decode_organization_catalog_v6(const BoundaryValue& input,const CatalogIdentity& identity,bool named){auto result=decode_organization_catalog_v4(input,identity,named);if(result.ok)result.value.exact_fact_clock_enabled=true;return result;}
Status visit_organization_runtime_body_v6(CodecVisitor& c,OrganizationState& state,bool named,Integer service_effects_schema){
 if(service_effects_schema!=0&&service_effects_schema!=1)return c.reject("UnsupportedOrganizationServiceEffectsSchema");
 if(!c.reading()){auto valid=validate_organization_service_effects(state,service_effects_schema);if(!valid.ok)return valid;}
 return value(c,"organization",state,named,false,true,service_effects_schema);
}
Status emit_organization_runtime_body_v6_projection(ProjectionWriter& sink,std::string_view name,const OrganizationState& input,bool named,Integer service_effects_schema){
 auto valid=validate_organization_service_effects(input,service_effects_schema);if(!valid.ok)return valid;
 return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,input,[&](auto& c,auto& state){return record_value(c,state,named,false,true,service_effects_schema);});
}
Outcome<BoundaryValue> encode_organization_runtime_body_v6(const OrganizationState& input,bool named,Integer service_effects_schema){
 BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_organization_runtime_body_v6_projection(sink,"",input,named,service_effects_schema);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};
}
Outcome<OrganizationState> decode_organization_runtime_body_v6(const BoundaryValue& input,bool named,Integer service_effects_schema){
 ActorFoundationJsonVisitor c(input);
 if(service_effects_schema!=0&&service_effects_schema!=1)return {false,{},c.reject("UnsupportedOrganizationServiceEffectsSchema").error};
 auto valid=prepare_checkpoint_default_values<OrganizationState>(c);if(!valid.ok)return {false,{},valid.error};
 if(!ScopedCheckpointJsonDecode::current())if(auto* owner=ScopedCheckpointBoundaryDecode::current())owner->default_values<OrganizationState>();
 OrganizationState out;valid=record_value(c,out,named,false,true,service_effects_schema);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};
}

}
