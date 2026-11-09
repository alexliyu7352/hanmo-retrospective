#include <string_view>
// V既定organization物理4/5，仍原同一段；旧2/3编码入口保持原字节。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
class ProjectionWriter;

Outcome<OrganizationCatalog> decode_organization_catalog_v4(const BoundaryValue&,const CatalogIdentity&,bool named_membership=false);
Outcome<OrganizationState> decode_organization_state_v4(const BoundaryValue&,bool named_membership=false);
Outcome<BoundaryValue> encode_organization_state_v4(const OrganizationState&,bool named_membership=false);
Status visit_organization_checkpoint_v4(CodecVisitor&,OrganizationState&,bool named_membership=false);
Status emit_organization_state_v4_projection(ProjectionWriter&,std::string_view,const OrganizationState&,bool named);
// Explicit complete M5 composition: current_grants belongs only to runtime/grants.
// These entry points never emit/read that table in the original organization body.
Status visit_organization_runtime_body(CodecVisitor&,OrganizationState&,bool named_membership=false);
Status emit_organization_runtime_body_projection(ProjectionWriter&,std::string_view,const OrganizationState&,bool named_membership=false);
Outcome<BoundaryValue> encode_organization_runtime_body(const OrganizationState&,bool named_membership=false);
Outcome<OrganizationState> decode_organization_runtime_body(const BoundaryValue&,bool named_membership=false);

// 显式完整物理6/7（named=false/true）：原body字段后每fact追加必填occurred_at；不重复M5 grants。
// service_effects_schema=1 only permits the separately selected physical adjunct;
// it adds no body field. Base reads defer exhaustive presence until attachment.
Outcome<OrganizationCatalog> decode_organization_catalog_v6(const BoundaryValue&,const CatalogIdentity&,bool named_membership=false);
Status visit_organization_runtime_body_v6(CodecVisitor&,OrganizationState&,bool named_membership=false,Integer service_effects_schema=0);
Status emit_organization_runtime_body_v6_projection(ProjectionWriter&,std::string_view,const OrganizationState&,bool named_membership=false,Integer service_effects_schema=0);
Outcome<BoundaryValue> encode_organization_runtime_body_v6(const OrganizationState&,bool named_membership=false,Integer service_effects_schema=0);
Outcome<OrganizationState> decode_organization_runtime_body_v6(const BoundaryValue&,bool named_membership=false,Integer service_effects_schema=0);
}
