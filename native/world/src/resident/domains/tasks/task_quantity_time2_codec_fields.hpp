#pragma once
#include "task_quantity_time2.hpp"
#include "task_dynamic_codec.hpp"
namespace hanmo::resident::task_quantity_time2_codec {
template<class A>Status source(A&a,SourceRef&v){a.field("domain",v.domain);a.field("kind",v.kind);return a.field("id",v.id);}
// Same optional presence representation as the original visitor; writing borrows
// the real string instead of allocating an optional copy of it.
template<class A>Status optional_text(A&a,const StableId&key,StableId&v){return a.object(key,[&](auto&c){bool present=!v.empty();c.field("present",present);if(!c.status().ok)return c.status();if(c.reading()&&!present)v.clear();if(present){auto s=c.field("value",v);if(s.ok&&v.empty())return c.reject("EmptyExactQuantityOptionalReference",key);return s;}return c.status();});}
// T3.2/3.3 quantity leaves of the explicit source2 union. Other original
// Aid/Professional tags belong to their actual typed producer and are not
// accepted through this quantity-only address.
template<class A,class T,class F>Status quantity_source(A&a,T&v,const StableId&tag_field,F payload){
 Integer schema=2,tag=static_cast<Integer>(v.quantity_source_kind);a.field("source_schema",schema);a.field(tag_field,tag);if(schema!=2||tag<0||tag>1)return a.reject("TaskQuantitySource2BranchRequired");if(a.reading())v.quantity_source_kind=static_cast<TaskQuantitySourceKind>(tag);return a.object("payload",[&](auto&c){return payload(c,v,tag==1);});
}
template<class A>Status delivery_source2(A&a,DeliveryFact&v){return quantity_source(a,v,"delivery_kind",[](auto&c,auto&x,bool exact){return dynamic_codec::delivery(c,x,exact);});}
template<class A>Status transfer_source2(A&a,TaskTransferFact&v){return quantity_source(a,v,"transfer_kind",[](auto&c,auto&x,bool exact){return dynamic_codec::transfer(c,x,exact);});}
template<class A>Status record(A&a,TaskRecord&t){
 Integer kind=static_cast<Integer>(t.record_kind);a.field("record_kind",kind);if(kind!=static_cast<Integer>(TaskRecordKind::exact_quantity))return a.reject("ExactQuantityRecordKindRequired");
 if(a.reading()){t.record_kind=TaskRecordKind::exact_quantity;t.exact_quantity.emplace();t.offer_ref.emplace();t.contract_id.emplace();}
 if(!t.exact_quantity||!t.offer_ref||!t.contract_id)return a.reject("ExactQuantityRecordFieldsRequired");auto&q=*t.exact_quantity;
 a.field("id",t.id);a.object("offer_ref",[&](auto&c){return dynamic_codec::offer_ref(c,*t.offer_ref);});
 a.object("assignee",[&](auto&c){return dynamic_codec::subject(c,q.assignee);});a.field("executor_actor_ref",t.assignee);a.field("serial",t.serial);a.field("status",t.status);a.field("budget_wallet_id",t.budget_wallet_id);
 a.optional("accepted_at",t.accepted_at,[](auto&c,auto&v){return dynamic_codec::exact_moment(c,v);});a.field("original_acceptance_action_ref",q.original_acceptance_action_ref);
 a.object("deadline_gate",[&](auto&c){c.field("tick",q.deadline_gate.tick);return c.field("inclusive",q.deadline_gate.inclusive);});optional_text(a,"settlement_id",t.settlement_id);
 a.optional("objective_completed_at",q.objective_completed_at,[](auto&c,auto&v){return dynamic_codec::exact_moment(c,v);});a.optional("reported_at",q.reported_at,[](auto&c,auto&v){return dynamic_codec::exact_moment(c,v);});a.optional("late",t.late,[](auto&c,auto&v){return c.field("value",v);});a.field("contract_id",*t.contract_id);
 a.optional("original_acceptance_result_ref",q.original_acceptance_result_ref,[](auto&c,auto&v){return source(c,v);});a.optional("original_report_result_ref",q.original_report_result_ref,[](auto&c,auto&v){return source(c,v);});
 if(!a.status().ok)return a.status();if(t.id.empty()||t.assignee.empty()||t.budget_wallet_id.empty()||t.contract_id->empty()||q.assignee.id.empty()||q.original_acceptance_action_ref.empty()||t.serial<1||q.deadline_gate.tick<0||!q.deadline_gate.inclusive||t.escort)return a.reject("InvalidExactQuantityTaskRecord");
 // These are legacy compatibility members, not fields of the exact wire.
 // Cold exact consumers use the actual moments and original result bindings.
 if(a.reading()){t.offer_id=t.offer_ref->id;t.accepted_tick=0;t.deadline_tick=q.deadline_gate.tick;t.objective_completed_tick.reset();t.reported_tick.reset();t.field_order.clear();}
 return a.status();
}
}
