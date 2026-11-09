#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "work_commission_terminal.hpp"
#include "work_follow_through_codec.hpp"
#include "../task_dynamic_projection.hpp"
namespace hanmo::resident {namespace {
template<class A>Status fields(A&a,TaskServiceTerminalState&s){a.field("version",s.version);if(s.version!=1)return a.reject("UnsupportedTaskServiceTerminalVersion");return dynamic_codec::table(a,"work_commission_terminals",s.work_commission_terminals,[](auto&c,auto&v){return work_follow_through_codec::task_terminal(c,v);});}
}
Status visit_task_service_terminals(CodecVisitor&a,TaskServiceTerminalState&s){return fields(a,s);}
Outcome<TaskServiceTerminalState> decode_task_service_terminals(const BoundaryValue&v){TaskServiceTerminalState s;dynamic_codec::JsonArchive a(v);auto status=fields(a,s);if(status.ok)status=a.finish();return status.ok?Outcome<TaskServiceTerminalState>{true,std::move(s),{}}:Outcome<TaskServiceTerminalState>{false,{},status.error};}
Outcome<BoundaryValue> encode_task_service_terminals(const TaskServiceTerminalState&s){return dynamic_projection::encode(s,[](auto&a,auto&v){return fields(a,v);});}
Status emit_task_service_terminals_projection(ProjectionWriter&w,std::string_view name,const TaskServiceTerminalState&s){return dynamic_projection::emit(w,name,s,[](auto&a,auto&v){return fields(a,v);});}
}
