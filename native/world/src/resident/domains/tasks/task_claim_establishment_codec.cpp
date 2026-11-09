#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "task_claim_establishment.hpp"
#include "task_model.hpp"
#include "task_dynamic_codec.hpp"
namespace hanmo::resident {
namespace {
template<class A>Status source(A& a,SourceRef& v){a.field("domain",v.domain);a.field("kind",v.kind);return a.field("id",v.id);}
template<class A>Status moment(A& a,WorldMoment& v){return dynamic_codec::exact_moment(a,v);}
template<class A>Status record(A&c,TaskClaimEstablishedSource&v){
 c.field("source_id",v.source_id);c.field("task_id",v.task_id);c.field("contract_id",v.contract_id);c.field("claim_id",v.claim_id);c.field("acceptance_action_id",v.acceptance_action_id);c.object("established_at",[&](auto&x){return moment(x,v.established_at);});c.object("committed_result_ref",[&](auto&x){return source(x,v.committed_result_ref);});
 if(c.status().ok&&(v.source_id.empty()||v.task_id.empty()||v.contract_id.empty()||v.claim_id.empty()||v.acceptance_action_id.empty()||v.committed_result_ref.domain!="common"||v.committed_result_ref.kind!="command_result"||v.committed_result_ref.id.empty()))return c.reject("TaskClaimCommittedOriginalSourceRequired");return c.status();
}
template<class A>Status fields(A& a,TaskClaimEstablishmentState& s){return dynamic_codec::table(a,"claim_establishment_sources",s.claim_establishment_sources,[](auto&c,auto&v){return record(c,v);});}
}
Outcome<TaskClaimEstablishmentState> decode_task_claim_establishment(const BoundaryValue& value){TaskClaimEstablishmentState s;dynamic_codec::JsonArchive a(value);auto r=fields(a,s);if(r.ok)r=a.finish();return r.ok?Outcome<TaskClaimEstablishmentState>{true,std::move(s),{}}:Outcome<TaskClaimEstablishmentState>{false,{},r.error};}
Status emit_task_claim_establishment_projection(ProjectionWriter&sink,std::string_view key,const TaskClaimEstablishmentState&value){return dynamic_projection::emit(sink,key,value,[](auto&a,auto&v){return fields(a,v);});}
Outcome<BoundaryValue> encode_task_claim_establishment(const TaskClaimEstablishmentState&value){return dynamic_projection::encode(value,[](auto&a,auto&v){return fields(a,v);});}
Status visit_task_claim_establishment_checkpoint(CodecVisitor& a,TaskClaimEstablishmentState& s){return fields(a,s);}
Outcome<bool> emit_task_claim_established_fact_payload(ProjectionWriter&w,const TaskState&tasks,const FactRecord&h){
 if(h.kind!="task_claim_established")return {true,false,{}};
 const auto*s=tasks.claim_establishment?tasks.claim_establishment->claim_establishment_sources.find(h.id):nullptr;
 if(h.domain!="tasks"||!tasks.exact_quantities||!s||s->source_id!=h.id)return {false,{},common_failure("TaskClaimOriginalPayloadMissing",h.id,ErrorKind::internal).error};
 dynamic_projection::Archive archive(w);auto valid=record(archive,const_cast<TaskClaimEstablishedSource&>(*s));return valid.ok?Outcome<bool>{true,true,{}}:Outcome<bool>{false,{},valid.error};
}

}
