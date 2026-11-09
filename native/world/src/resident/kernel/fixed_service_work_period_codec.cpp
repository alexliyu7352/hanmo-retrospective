// 严格同表出口：JSON reader 借原树，projection 借原 record，不构造临时 period 镜像。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "fixed_service_work_period_codec.hpp"
#include "fixed_service_work_period_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/model/common_model.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
template<class T,class F>Outcome<T> guarded(F f){try{return {true,f(),{}};}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}catch(const std::bad_alloc&){return {false,{},common_failure("FixedPeriodCodecResourceExhausted","work_period/2",ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("FixedPeriodCodecResourceExhausted","work_period/2",ErrorKind::internal).error};}}
template<class T,class F>Outcome<T> decode(const BoundaryValue& raw,F fields){return guarded<T>([&]{T value;ActorFoundationJsonVisitor c(raw);exact_fields::take(fields(c,value));exact_fields::take(c.finish());return value;});}
template<class T,class F>Status emit(ProjectionWriter& sink,std::string_view name,const T& value,F fields){return guarded<Done>([&]{exact_fields::take(emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,fields));return Done{};});}
template<class T,class F>Outcome<BoundaryValue> encode(const T& value,F fields){return guarded<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);exact_fields::take(emit(sink,"",value,fields));exact_fields::take(sink.finish());return out;});}
template<class T,class F>Status visit(CodecVisitor& c,T& value,F fields){return guarded<Done>([&]{exact_fields::take(fields(c,value));return Done{};});}
}
#define FIXED_PERIOD_CODEC(TYPE,NAME,FIELDS) \
Outcome<BoundaryValue> encode_##NAME(const TYPE& v){return encode(v,[](auto& c,auto& x){return fixed_period_fields::FIELDS(c,x);});} \
Outcome<TYPE> decode_##NAME(const BoundaryValue& v){return decode<TYPE>(v,[](auto& c,auto& x){return fixed_period_fields::FIELDS(c,x);});} \
Status visit_##NAME(CodecVisitor& c,TYPE& v){return visit(c,v,[](auto& x,auto& row){return fixed_period_fields::FIELDS(x,row);});} \
Status emit_##NAME(ProjectionWriter& c,std::string_view name,const TYPE& v){return emit(c,name,v,[](auto& x,auto& row){return fixed_period_fields::FIELDS(x,row);});}
FIXED_PERIOD_CODEC(WorkPeriodRef2,work_period_ref2,ref)
FIXED_PERIOD_CODEC(WorkPeriodView2,work_period_view2,view)
#undef FIXED_PERIOD_CODEC
}
