// 仅核S5消费已证明Task接触DTO的边界；人工读源不冒真实Task接受/交货/资金或World验收。
#define main private_shop_original_fixture_main
#include "private_shop_transaction_test.cpp"
#undef main
#include "resident/domains/commerce/private_shop_order_contact.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_contract.hpp"
struct ContactRead final:TaskShopOrderContactRead {
 std::optional<TaskShopOrderDeliveryContactSource> source;mutable Integer calls=0;
 Outcome<std::optional<TaskShopOrderDeliveryContactSource>> task_shop_order_delivery_contact(const StableId& task,const StableId& settled)const override{++calls;if(source&&(task!=source->contact.task_id||settled!=source->settlement_fact_id))return {false,{},common_failure("UnexpectedContactReadScope").error};return {true,source,{}};}
};
// 原完整成本入口启用即拒缺服务，纯结构入口保持无接触回调；拒绝发生在任何新ID前。
void selected_contact_gate(){Fixture f;auto x=f.context();const auto original=encode_common_state(f.common.state());
 check(validate_private_shop_order_settlements(x,true).ok,"old profile needs no new contact service");
 x.order_contacts_required=true;auto invalid=validate_private_shop_order_settlements(x,true);check(!invalid.ok&&invalid.error.original.code=="PrivateShopOrderContactHistoryUnavailable","selected full cost validation cannot omit contact service even before first delivery");
 check(validate_private_shop_order_settlements(x,false).ok,"original structural source stays nonrecursive before contact history");
 auto captured=capture_private_shop_order_execution_in_candidate(f.common,f.state,x,"unissued-order");check(!captured.ok&&captured.error.original.code=="PrivateShopOrderContactHistoryUnavailable","selected capture rejects missing service before allocating any cost source");
 check(compare_boundary(original,encode_common_state(f.common.state())).exact(),"missing selected service changes no assets facts or allocation");
 ContactRead read;x.order_contacts=&read;check(validate_private_shop_order_settlements(x,true).ok&&read.calls==0,"empty selected ledger requires service but invents no delivery read");
}
int main(){try{selected_contact_gate();
 Fixture f;auto fact=[&](const StableId& prefix,const StableId& kind,const StableId& domain){auto id=f.common.next_id(prefix);check(id.ok&&f.common.append_fact({id.value,kind,domain}).ok,"allocate fixture source header");return id.value;};
 PrivateShopOrderRecord order;order.id=fact("private_shop_order","private_shop_order_issued","private_shops");order.status="accepted";order.task_id=f.common.next_id("task").value;order.acceptance_fact_id=fact("task_shop_order_accepted","task_shop_order_accepted","task_shop_orders");order.binding_fact_id=fact("private_shop_order_bound","private_shop_order_bound","private_shops");order.terms.order_id=order.id;order.terms.proprietor_actor_id="owner";order.terms.original_client={Owner::Kind::actor,"lessor",{}};order.terms.accepted_tick=0;order.client_declaration={"owner","lessor","old-meeting","lease","agree","market","lessor.role"};f.state.orders.orders.insert(order.id,order);const auto& original=*f.state.orders.orders.find(order.id);
 ContactRead read;TaskShopOrderDeliveryContactSource source;auto& a=source.contact;a.task_id=order.task_id;a.order_id=order.id;a.acceptance_fact_id=order.acceptance_fact_id;a.order_issue_fact_id=order.id;a.proprietor_actor_id="owner";a.client_actor_id="lessor";a.action_id=f.common.next_id("action").value;a.fact_id=fact(task_shop_order_contact_started_kind,task_shop_order_contact_started_kind,task_shop_order_contact_module);a.session_id="new-meeting";a.dialogue_id="lease";a.node_id="agree";a.context={"market","lessor","lessor.role"};a.actual_result=TaskShopOrderContactResult{f.common.next_id("result").value,"owner",1,1};source.settlement_id=f.common.next_id("settlement").value;source.settlement_fact_id=fact("task_shop_order_settled","task_shop_order_settled","task_shop_orders");source.completed_tick=1;f.common.state().identity.revision=1;f.common.state().identity.tick=1;source.source_identity=f.common.state().identity;read.source=source;
 PrivateShopOrderExecution execution;execution.source_identity=f.common.state().identity;execution.order_id=order.id;execution.task_id=order.task_id;execution.receipt_fact_id=source.settlement_fact_id;execution.settlement_id=source.settlement_id;execution.completed_tick=1;execution.outcome="delivered";auto x=f.context();
 check(validate_private_shop_order_delivery_contact(x,original,execution).ok,"old delivery caller preserves legacy source path");x.order_contacts_required=true;
 check(!validate_private_shop_order_delivery_contact(x,original,execution).ok,"selected delivery cannot silently bypass missing contact read");x.order_contacts=&read;
 check(validate_private_shop_order_delivery_contact(x,original,execution).ok&&read.calls==1,"selected delivery invokes exact source consumer");
 check(validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"exact original contact DTO joins original order and settlement");check(f.common.state().results.empty(),"consumer never requires this completion's not-yet-allocated cache");
 auto good=*read.source;read.source.reset();check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"missing contact source is unavailable");read.source=good;
 read.source->contact.actual_result.reset();check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"previous start must already carry actual result");read.source=good;
 read.source->contact.context.role_id="another-role";check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"another actual speaker role cannot replace original customer declaration");read.source=good;
 read.source->contact.node_id="another-say";check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"another say cannot replace original customer request");read.source=good;
 read.source->contact.acceptance_fact_id="task_shop_order_accepted:1";check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"another acceptance is not this contract");read.source=good;
 read.source->contact.actual_result->result_id=source.settlement_fact_id;check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"completion cannot stand in for earlier start result");read.source=good;
 execution.outcome="cancelled";const auto calls=read.calls;check(validate_private_shop_order_delivery_contact(x,original,execution).ok&&read.calls==calls,"cancelled contract requires no successful-delivery contact");check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"cancelled order has no successful delivery contact");execution.outcome="delivered";
 f.common.state().identity.tick=0;execution.source_identity=f.common.state().identity;read.source->source_identity=f.common.state().identity;check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"cold current clock cannot read future completion");DynamicStepInput phase;phase.from_tick=0;phase.to_tick=1;x.phase=&phase;check(validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"real one-step phase permits immediate pre-clock cost capture");phase.to_tick=2;check(!validate_private_shop_order_delivery_contact(x,original,execution,read).ok,"phase cannot extend arbitrary future history");
 std::cout<<checks<<"/0 shop contact consumer checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
