#pragma once
#include "material_aid_model.hpp"
namespace hanmo::resident {
class CodecVisitor;class ProjectionWriter;
Status validate_treaty_material_aid_shape(const TreatyMaterialAidState&);
Outcome<TreatyMaterialAidState>decode_treaty_material_aid(const BoundaryValue&);
Outcome<BoundaryValue>encode_treaty_material_aid(const TreatyMaterialAidState&);
Status visit_treaty_material_aid_checkpoint(CodecVisitor&,TreatyMaterialAidState&);
Status emit_treaty_material_aid_projection(ProjectionWriter&,const StableId&,const TreatyMaterialAidState&);
Status emit_material_aid_original_fact_payload(ProjectionWriter&,const StableId&,
 const CommonStore&,const TreatyMaterialAidState&,const FactRecord&);
}
