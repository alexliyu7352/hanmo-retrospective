// /5 原九字段不变；/6 明确追加首窗接受源，仍复用同一原字段编码器。
#pragma once
#include "work_supervision_execution_terms.hpp"
#include "work_result_source_codec.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
namespace hanmo::resident::supervision_execution_codec {
template<class A,class V>Status source(A&a,V&v){a.object("source_ref",[&](auto&c){c.field("domain",v.source_ref.domain);c.field("kind",v.source_ref.kind);return c.field("id",v.source_ref.id);});return a.field("source_schema",v.source_schema);}
template<class A,class V>Status fields(A&a,V&v,Integer selected_version=5){
 if(!task_work_fixed_execution_terms_version(selected_version))return a.reject("SupervisionExecutionTermsRequirePhysical5");
 a.field("work_id",v.work_id);a.field("execution_ordinal",v.execution_ordinal);
 a.object("original_authorization_ref",[&](auto&c){return work_source_codec::authorization(c,v.original_authorization_ref);});
 a.object("original_approved_execution_terms_ref",[&](auto&c){return source(c,v.original_approved_execution_terms_ref);});
 a.field("action_id",v.action_id);dynamic_codec::enumeration(a,"kind",v.kind,3);a.field("service_limit_half_days",v.service_limit_half_days);
 a.optional("actual_start_source_ref",v.actual_start_source_ref,[](auto&c,auto&x){return source(c,x);});
 a.optional("actual_terminal_result_ref",v.actual_terminal_result_ref,[](auto&c,auto&x){return source(c,x);});
 if(task_work_lifecycle_sources_version(selected_version)){a.optional("initial_successor_acceptance_ref",v.initial_successor_acceptance_ref,[](auto&c,auto&x){c.field("domain",x.domain);c.field("kind",x.kind);return c.field("id",x.id);});}
 else if(v.initial_successor_acceptance_ref)return a.reject("WorkReassignmentAcceptanceRequiresPhysical6");
 if(!a.status().ok)return a.status();auto valid=validate_supervision_execution_terms_shape(v);return valid.ok?a.status():a.reject(valid.error.original.code);
}
}
