// Physical ownership is selected before any body field is read. Legacy
// organization-only 2/3/4/5 callers retain their original entry points/bytes.
#include "organization_runtime.hpp"
#include "organization_authority_codec.hpp"
namespace hanmo::resident {
Status visit_organization_runtime_selected_body(CodecVisitor& codec,OrganizationState& state,const CatalogIdentity& identity){
 auto selected=select_organization_runtime_codec(identity);if(!selected.ok)return codec.reject(selected.error.original.code,selected.error.original.path);
 return selected.value.exact_fact_clock?visit_organization_runtime_body_v6(codec,state,selected.value.named_membership,selected.value.service_effects_schema):visit_organization_runtime_body(codec,state,selected.value.named_membership);
}
Status emit_organization_runtime_selected_body_projection(ProjectionWriter& writer,std::string_view name,const OrganizationState& state,const CatalogIdentity& identity){
 auto selected=select_organization_runtime_codec(identity);if(!selected.ok)return {false,{},selected.error};
 return selected.value.exact_fact_clock?emit_organization_runtime_body_v6_projection(writer,name,state,selected.value.named_membership,selected.value.service_effects_schema):emit_organization_runtime_body_projection(writer,name,state,selected.value.named_membership);
}
Outcome<BoundaryValue> encode_organization_runtime_selected_body(const OrganizationState& state,const CatalogIdentity& identity){
 auto selected=select_organization_runtime_codec(identity);if(!selected.ok)return {false,{},selected.error};
 return selected.value.exact_fact_clock?encode_organization_runtime_body_v6(state,selected.value.named_membership,selected.value.service_effects_schema):encode_organization_runtime_body(state,selected.value.named_membership);
}
Outcome<OrganizationState> decode_organization_runtime_selected_body(const BoundaryValue& input,const CatalogIdentity& identity){
 auto selected=select_organization_runtime_codec(identity);if(!selected.ok)return {false,{},selected.error};
 return selected.value.exact_fact_clock?decode_organization_runtime_body_v6(input,selected.value.named_membership,selected.value.service_effects_schema):decode_organization_runtime_body(input,selected.value.named_membership);
}
}
