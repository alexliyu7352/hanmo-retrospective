#include "treaty_aid_resource_selection_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_aid_resource_selection(CodecVisitor&c,TreatyAidResourceSelection&r){return treaty_aid_selection_fields::fields(c,r);}
Status emit_treaty_aid_resource_selection_projection(ProjectionWriter&w,std::string_view name,const TreatyAidResourceSelection&r){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,r,[](auto&c,auto&v){return treaty_aid_selection_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_aid_resource_selection(const TreatyAidResourceSelection&r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_aid_resource_selection_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyAidResourceSelection> decode_treaty_aid_resource_selection(const BoundaryValue&input){TreatyAidResourceSelection out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_aid_selection_fields::fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
