#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "service_actual_termination_result_fields.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
namespace hanmo::resident {
Outcome<ServiceActualTerminationResult> decode_service_actual_termination_result(const BoundaryValue&value,Integer schema){ServiceActualTerminationResult out;dynamic_codec::JsonArchive a(value);auto status=service_termination_codec::result(a,out,schema);if(status.ok)status=a.finish();return status.ok?Outcome<ServiceActualTerminationResult>{true,std::move(out),{}}:Outcome<ServiceActualTerminationResult>{false,{},status.error};}
Outcome<BoundaryValue> encode_service_actual_termination_result(const ServiceActualTerminationResult&value,Integer schema){return dynamic_projection::encode(value,[&](auto&a,auto&v){return service_termination_codec::result(a,v,schema);});}
Status visit_service_actual_termination_result_checkpoint(CodecVisitor&a,ServiceActualTerminationResult&v,Integer schema){return service_termination_codec::result(a,v,schema);}
Status emit_service_actual_termination_result_fields(ProjectionWriter&w,const ServiceActualTerminationResult&v,Integer schema){DynamicProjectionArchive a(w);return service_termination_codec::result(a,const_cast<ServiceActualTerminationResult&>(v),schema);}
}
