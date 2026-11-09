// Original Organization JSON body. The caller supplies the actual legacy named selector.
#pragma once
#include "organization_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_organization_state_fields(ProjectionWriter&,const OrganizationState&,bool named_membership=false);
Status emit_organization_state_projection(ProjectionWriter&,std::string_view,const OrganizationState&,bool named_membership=false);
}
