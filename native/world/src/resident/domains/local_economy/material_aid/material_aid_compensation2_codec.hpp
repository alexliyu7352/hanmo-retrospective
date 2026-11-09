#pragma once
#include "material_aid_compensation2.hpp"
#include "material_aid_codec.hpp"
namespace hanmo::resident {
Status validate_treaty_material_aid_values2(const TreatyMaterialAidState2&);
Status initialize_treaty_material_aid2(TreatyMaterialAidState2&);
Outcome<TreatyMaterialAidState2>decode_treaty_material_aid2(const BoundaryValue&);
Outcome<BoundaryValue>encode_treaty_material_aid2(const TreatyMaterialAidState2&);
Status visit_treaty_material_aid_checkpoint2(CodecVisitor&,TreatyMaterialAidState2&);
Status emit_treaty_material_aid_projection2(ProjectionWriter&,const StableId&,const TreatyMaterialAidState2&);
Status emit_material_aid_compensation_fact_payload2(ProjectionWriter&,const StableId&,const CommonStore&,const TreatyMaterialAidState2&,const FactRecord&);
}
