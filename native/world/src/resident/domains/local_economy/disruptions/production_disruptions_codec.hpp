#pragma once
#include "production_disruptions.hpp"
namespace hanmo::resident {
Outcome<ProductionDisruptionCatalog> decode_production_disruption_catalog(const BoundaryValue&);
BoundaryValue encode_production_disruption_catalog(const ProductionDisruptionCatalog&);
Outcome<ProductionDisruptionState> decode_production_disruption_state(const BoundaryValue&);
class ProjectionWriter;
Status emit_production_disruption_state_fields(ProjectionWriter&,const ProductionDisruptionState&);
BoundaryValue encode_production_disruption_state(const ProductionDisruptionState&);
Status visit_production_disruption_checkpoint(CodecVisitor&,ProductionDisruptionState&);
}
