#pragma once
#include "dynamic_objective_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <string_view>
namespace hanmo::resident {
class ProjectionWriter;struct MilitaryExactState;
Status visit_dynamic_military_objective_evidence(CodecVisitor&,DynamicMilitaryObjectiveEvidence2&);
Status visit_dynamic_military_objective_evidence_table(CodecVisitor&,MilitaryExactState&);
Outcome<DynamicMilitaryObjectiveEvidence2> decode_dynamic_military_objective_evidence(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_military_objective_evidence(const DynamicMilitaryObjectiveEvidence2&);
Status emit_dynamic_military_objective_evidence(ProjectionWriter&,std::string_view,const DynamicMilitaryObjectiveEvidence2&);
}
