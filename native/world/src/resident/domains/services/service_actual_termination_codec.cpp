#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "service_actual_termination_codec_fields.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
namespace hanmo::resident {
Outcome<AcceptedServiceScopeRef>decode_accepted_service_scope(const BoundaryValue&value,Integer schema){AcceptedServiceScopeRef out;dynamic_codec::JsonArchive a(value);auto status=service_termination_codec::scope(a,out,schema);if(status.ok)status=a.finish();return status.ok?Outcome<AcceptedServiceScopeRef>{true,std::move(out),{}}:Outcome<AcceptedServiceScopeRef>{false,{},status.error};}
Outcome<BoundaryValue>encode_accepted_service_scope(const AcceptedServiceScopeRef&value,Integer schema){return dynamic_projection::encode(value,[&](auto&a,auto&v){return service_termination_codec::scope(a,v,schema);});}
Status visit_accepted_service_scope_checkpoint(CodecVisitor&a,AcceptedServiceScopeRef&v,Integer schema){return service_termination_codec::scope(a,v,schema);}
Status emit_accepted_service_scope_fields(ProjectionWriter&w,const AcceptedServiceScopeRef&v,Integer schema){DynamicProjectionArchive a(w);return service_termination_codec::scope(a,const_cast<AcceptedServiceScopeRef&>(v),schema);}
Outcome<OriginalServiceTerminalRef>decode_original_service_terminal(const BoundaryValue&value,Integer schema){OriginalServiceTerminalRef out;dynamic_codec::JsonArchive a(value);auto status=service_termination_codec::terminal(a,out,schema);if(status.ok)status=a.finish();return status.ok?Outcome<OriginalServiceTerminalRef>{true,std::move(out),{}}:Outcome<OriginalServiceTerminalRef>{false,{},status.error};}
Outcome<BoundaryValue>encode_original_service_terminal(const OriginalServiceTerminalRef&value,Integer schema){return dynamic_projection::encode(value,[&](auto&a,auto&v){return service_termination_codec::terminal(a,v,schema);});}
Status visit_original_service_terminal_checkpoint(CodecVisitor&a,OriginalServiceTerminalRef&v,Integer schema){return service_termination_codec::terminal(a,v,schema);}
Status emit_original_service_terminal_fields(ProjectionWriter&w,const OriginalServiceTerminalRef&v,Integer schema){DynamicProjectionArchive a(w);return service_termination_codec::terminal(a,const_cast<OriginalServiceTerminalRef&>(v),schema);}
}
