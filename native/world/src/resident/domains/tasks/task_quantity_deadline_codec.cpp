// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_quantity_deadline_codec_fields.hpp"
#include "task_dynamic_projection.hpp"
namespace hanmo::resident {
Outcome<TaskQuantityDeadlineClosure> decode_task_quantity_deadline_closure(const BoundaryValue&b){TaskQuantityDeadlineClosure v;dynamic_codec::JsonArchive a(b);auto s=task_quantity_deadline_codec::closure(a,v);if(s.ok)s=a.finish();return s.ok?Outcome<TaskQuantityDeadlineClosure>{true,std::move(v),{}}:Outcome<TaskQuantityDeadlineClosure>{false,{},s.error};}
Outcome<BoundaryValue> encode_task_quantity_deadline_closure(const TaskQuantityDeadlineClosure&v){return dynamic_projection::encode(v,[](auto&a,auto&x){return task_quantity_deadline_codec::closure(a,x);});}
Status visit_task_quantity_deadline_closure(CodecVisitor&a,TaskQuantityDeadlineClosure&v){return task_quantity_deadline_codec::closure(a,v);}
Status emit_task_quantity_deadline_closure_fields(ProjectionWriter&w,const TaskQuantityDeadlineClosure&v){DynamicProjectionArchive a(w);return task_quantity_deadline_codec::closure(a,const_cast<TaskQuantityDeadlineClosure&>(v));}
Outcome<ContractRecord> decode_quantity_contract_record(const BoundaryValue&b,Integer version){ContractRecord v;dynamic_codec::JsonArchive a(b);auto s=task_quantity_deadline_codec::contract(a,v,version);if(s.ok)s=a.finish();return s.ok?Outcome<ContractRecord>{true,std::move(v),{}}:Outcome<ContractRecord>{false,{},s.error};}
Outcome<BoundaryValue> encode_quantity_contract_record(const ContractRecord&v,Integer version){return dynamic_projection::encode(v,[&](auto&a,auto&x){return task_quantity_deadline_codec::contract(a,x,version);});}
Status visit_quantity_contract_record(CodecVisitor&a,ContractRecord&v,Integer version){return task_quantity_deadline_codec::contract(a,v,version);}
Status emit_quantity_contract_record_fields(ProjectionWriter&w,const ContractRecord&v,Integer version){DynamicProjectionArchive a(w);return task_quantity_deadline_codec::contract(a,const_cast<ContractRecord&>(v),version);}
Outcome<bool> emit_task_quantity_deadline_fact_payload(ProjectionWriter&w,const TaskState&t,const FactRecord&h){
 const TaskQuantityDeadlineClosure*found=nullptr;for(const auto&row:t.contracts.rows())if(row.second.deadline_closure&&row.second.deadline_closure->source_id==h.id){if(found||row.first!=row.second.id)return {false,{},common_failure("QuantityDeadlineDuplicateSource",h.id,ErrorKind::internal).error};found=&*row.second.deadline_closure;}
 if(h.kind!=task_quantity_deadline_closed_kind){if(found)return {false,{},common_failure("QuantityDeadlineHeaderMismatch",h.id,ErrorKind::internal).error};return {true,false,{}};}
 if(t.dynamic_tasks_version!=4||h.domain!="tasks"||!found)return {false,{},common_failure("QuantityDeadlineHeaderMismatch",h.id,ErrorKind::internal).error};auto s=emit_task_quantity_deadline_closure_fields(w,*found);return s.ok?Outcome<bool>{true,true,{}}:Outcome<bool>{false,{},s.error};
}
}
