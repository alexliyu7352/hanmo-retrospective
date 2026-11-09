#pragma once
#include "naval_repair_exact_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Leaf only. The final annex selector still requires its complete roots.
Status visit_exact_naval_repair_source(CodecVisitor&,NewNavalRepairTimeSource2&);
Status emit_exact_naval_repair_source(ProjectionWriter&,std::string_view,const NewNavalRepairTimeSource2&);
Outcome<BoundaryValue> encode_exact_naval_repair_source(const NewNavalRepairTimeSource2&);
Outcome<NewNavalRepairTimeSource2> decode_exact_naval_repair_source(const BoundaryValue&);
}
