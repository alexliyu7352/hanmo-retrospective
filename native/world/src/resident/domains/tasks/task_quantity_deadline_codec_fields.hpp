#pragma once
#include "task_quantity_deadline.hpp"
#include "task_dynamic_codec.hpp"
namespace hanmo::resident::task_quantity_deadline_codec {
template<class A>Status source(A&a,MomentOwnerSource&v){a.object("source_ref",[&](auto&c){c.field("domain",v.source_ref.domain);c.field("kind",v.source_ref.kind);return c.field("id",v.source_ref.id);});return a.field("source_schema",v.source_schema);}
template<class A>Status closure(A&a,TaskQuantityDeadlineClosure&v){
 a.field("source_id",v.source_id);a.field("task_id",v.task_id);
 a.object("original_acceptance_start_ref",[&](auto&c){return source(c,v.original_acceptance_start_ref);});
 a.object("frozen_deadline_gate",[&](auto&c){c.field("tick",v.frozen_deadline_gate.tick);return c.field("inclusive",v.frozen_deadline_gate.inclusive);});
 a.object("original_period_opening",[&](auto&c){return source(c,v.original_period_opening);});
 a.object("closed_at",[&](auto&c){return dynamic_codec::exact_moment(c,v.closed_at);});
 if(!a.status().ok)return a.status();if(v.source_id.empty()||v.task_id.empty()||v.frozen_deadline_gate.tick<0||!v.frozen_deadline_gate.inclusive||v.original_acceptance_start_ref.source_schema!=1||v.original_acceptance_start_ref.source_ref.domain!="tasks"||v.original_acceptance_start_ref.source_ref.kind!="task_action_started"||v.original_acceptance_start_ref.source_ref.id.empty()||v.original_period_opening.source_schema!=1||v.original_period_opening.source_ref.domain!="social"||v.original_period_opening.source_ref.kind!="social_period_open"||v.original_period_opening.source_ref.id.empty())return a.reject("InvalidQuantityDeadlineClosure");return a.status();
}
// The exact same original Quantity prefix. /3 refuses a /4 tail rather than
// silently erasing it. This is a payload visitor, not a whole-section shortcut.
template<class A>Status contract(A&a,ContractRecord&v,Integer version){
 if(version!=3&&version!=4)return a.reject("QuantityContractPhysicalVersionRequired");Integer kind=0;a.field("record_kind",kind);if(kind!=0)return a.reject("NativeObjectiveContractOwnerRequired");
 auto valid=dynamic_codec::contract_prefix(a,v);if(!valid.ok)return valid;
 if(version==4)return a.optional("deadline_closure",v.deadline_closure,[](auto&c,auto&x){return closure(c,x);});
 if(a.reading())v.deadline_closure.reset();return v.deadline_closure?a.reject("QuantityDeadlineRequiresDynamicTasks4"):a.status();
}
}
