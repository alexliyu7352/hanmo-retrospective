// 首次选型的建设根 codec；无旧档补造，完整源校验仍在候选安装出口执行。
#pragma once
#include "facility_construction_model.hpp"

namespace hanmo::resident {
struct CommonResourceCodecContext;
class CodecVisitor;
class ProjectionWriter;
Outcome<FacilityConstructionState>
decode_facility_construction(const BoundaryValue &, bool typed,
                             const CommonResourceCodecContext &);
Outcome<BoundaryValue>
encode_facility_construction(const FacilityConstructionState &, bool typed,
                             const CommonResourceCodecContext &);
Status emit_facility_construction_fields(ProjectionWriter &,
                                         const FacilityConstructionState &,
                                         bool typed,
                                         const CommonResourceCodecContext &);
Status visit_facility_construction(CodecVisitor &, FacilityConstructionState &,
                                   bool typed,
                                   const CommonResourceCodecContext &);
Outcome<FacilityConstructionCatalog>
decode_facility_construction_catalog(const BoundaryValue &);
Outcome<BoundaryValue>
encode_facility_construction_catalog(const FacilityConstructionCatalog &);
Status
emit_facility_construction_catalog_fields(ProjectionWriter &,
                                          const FacilityConstructionCatalog &);
Status visit_facility_construction_catalog(CodecVisitor &,
                                           FacilityConstructionCatalog &);
} // namespace hanmo::resident
