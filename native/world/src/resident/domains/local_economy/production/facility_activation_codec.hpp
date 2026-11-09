// 首次应用根候选 codec，尚未进入 World 或 module registry。
#pragma once
#include "facility_activation_model.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
Outcome<ProductionWorkApplicationsState>
decode_production_work_applications(const BoundaryValue &);
Outcome<BoundaryValue>
encode_production_work_applications(const ProductionWorkApplicationsState &);
Status emit_production_work_applications_fields(
    ProjectionWriter &, const ProductionWorkApplicationsState &);
Status visit_production_work_applications(CodecVisitor &,
                                          ProductionWorkApplicationsState &);
} // namespace hanmo::resident
