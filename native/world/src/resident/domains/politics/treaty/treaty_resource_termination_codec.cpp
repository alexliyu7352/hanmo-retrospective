#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_resource_termination_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_resource_termination2(CodecVisitor& c,TreatyTerminationSource& r){return treaty_resource_termination_fields::fields(c,r);}
Status emit_treaty_resource_termination2_projection(ProjectionWriter& sink,std::string_view name,const TreatyTerminationSource& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_resource_termination_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_resource_termination2(const TreatyTerminationSource& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_resource_termination2_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyTerminationSource> decode_treaty_resource_termination2(const BoundaryValue& input){TreatyTerminationSource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_resource_termination_fields::fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
