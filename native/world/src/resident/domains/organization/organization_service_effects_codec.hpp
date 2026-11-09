#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
class ProjectionWriter;
// Structural/coverage validation only. These effects do not prove the original
// Action/contract/response authorization or historical role assignment.
Status validate_organization_service_effects(const OrganizationState&,Integer selected_schema);
Status validate_organization_service_effects_parent(const OrganizationFact&,const OrganizationServiceEffects&);
// Reader attaches to a private, already decoded exact Organization body.
// On any error discard that candidate; no partial attachment may be published.
Status decode_organization_service_effects(const BoundaryValue&,OrganizationState&,Integer selected_schema);
Outcome<BoundaryValue> encode_organization_service_effects(const OrganizationState&,Integer selected_schema);
Status visit_organization_service_effects_checkpoint(CodecVisitor&,OrganizationState&,Integer selected_schema);
Status emit_organization_service_effects_projection(ProjectionWriter&,std::string_view,const OrganizationState&,Integer selected_schema);
} // namespace hanmo::resident
