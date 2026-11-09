#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "naval_repair_exact_codec.hpp"
#include "naval_repair_exact_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_exact_naval_repair_source(CodecVisitor& c,NewNavalRepairTimeSource2& v){try{return naval_repair_exact_fields::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status emit_exact_naval_repair_source(ProjectionWriter& w,std::string_view n,const NewNavalRepairTimeSource2& v){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,n,v,[](auto& c,auto& row){return naval_repair_exact_fields::value(c,row);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_exact_naval_repair_source(const NewNavalRepairTimeSource2& v){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter w(out);movement_detail::take(emit_exact_naval_repair_source(w,"",v));movement_detail::take(w.finish());return out;});}
Outcome<NewNavalRepairTimeSource2> decode_exact_naval_repair_source(const BoundaryValue& raw){return movement_detail::guard<NewNavalRepairTimeSource2>([&]{try{NewNavalRepairTimeSource2 row;ActorFoundationJsonVisitor c(raw);movement_detail::take(naval_repair_exact_fields::value(c,row));movement_detail::take(c.finish());return row;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
}
