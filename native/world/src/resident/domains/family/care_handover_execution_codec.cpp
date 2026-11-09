#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_handover_execution_codec.hpp"
#include "care_handover_execution_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
namespace {
template<class T,class Emit>Outcome<BoundaryValue> encode_record(const T& v,Emit emit){BoundaryValue value;BoundaryProjectionWriter out(value);auto s=out.object("",[&](ProjectionWriter& w){return emit(w,v);});if(s.ok)s=out.finish();return {s.ok,std::move(value),s.error};}
template<class T,class Fields>Outcome<T> decode_record(const BoundaryValue& input,Fields fields){T value;ActorFoundationJsonVisitor c(input);auto s=fields(c,value);if(s.ok)s=c.finish();return {s.ok,std::move(value),s.error};}
}
Status visit_care_handover_execution(CodecVisitor& c,CareHandoverExecution& v,Integer version){return care_handover_codec::execution(c,v,version);}
Status emit_care_handover_execution_fields(ProjectionWriter& w,const CareHandoverExecution& v,Integer version){ActorFoundationProjectionArchive c(w);return care_handover_codec::execution(c,const_cast<CareHandoverExecution&>(v),version);}
Outcome<BoundaryValue> encode_care_handover_execution(const CareHandoverExecution& v,Integer version){return encode_record(v,[&](auto& w,const auto& row){return emit_care_handover_execution_fields(w,row,version);});}
Outcome<CareHandoverExecution> decode_care_handover_execution(const BoundaryValue& input,Integer version){return decode_record<CareHandoverExecution>(input,[&](auto& c,auto& row){return care_handover_codec::execution(c,row,version);});}
Status visit_care_handover_work_period2(CodecVisitor& c,CareHandoverWorkPeriod2& v){return care_handover_codec::period(c,v);}
Status emit_care_handover_work_period2_fields(ProjectionWriter& w,const CareHandoverWorkPeriod2& v){ActorFoundationProjectionArchive c(w);return care_handover_codec::period(c,const_cast<CareHandoverWorkPeriod2&>(v));}
Outcome<BoundaryValue> encode_care_handover_work_period2(const CareHandoverWorkPeriod2& v){return encode_record(v,emit_care_handover_work_period2_fields);}
Outcome<CareHandoverWorkPeriod2> decode_care_handover_work_period2(const BoundaryValue& input){return decode_record<CareHandoverWorkPeriod2>(input,[](auto& c,auto& row){return care_handover_codec::period(c,row);});}
Status visit_care_handover_execution_boundary1(CodecVisitor& c,CareHandoverExecutionBoundary1& v){return care_handover_codec::boundary(c,v);}
Status emit_care_handover_execution_boundary1_fields(ProjectionWriter& w,const CareHandoverExecutionBoundary1& v){ActorFoundationProjectionArchive c(w);return care_handover_codec::boundary(c,const_cast<CareHandoverExecutionBoundary1&>(v));}
Outcome<BoundaryValue> encode_care_handover_execution_boundary1(const CareHandoverExecutionBoundary1& v){return encode_record(v,emit_care_handover_execution_boundary1_fields);}
Outcome<CareHandoverExecutionBoundary1> decode_care_handover_execution_boundary1(const BoundaryValue& input){return decode_record<CareHandoverExecutionBoundary1>(input,[](auto& c,auto& row){return care_handover_codec::boundary(c,row);});}
}
