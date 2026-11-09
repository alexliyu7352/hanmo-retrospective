#include "shop_orders/task_shop_order_sources.hpp"
// 只证明未涉及或具名终结凭据已闭合的本人责任；公职经办不会把组织债务变成本人私债。
#include "task_inheritance_history.hpp"
#include "static_assets/task_static_asset.hpp"
#include <charconv>
namespace hanmo::resident { namespace {
std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.compare(0,head.size(),head)!=0)return {};const auto tail=id.substr(head.size());Integer v=0;const auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),v);if(tail.empty()||parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||v<=0||v>=next||std::to_string(v)!=tail)return {};return v;}
bool person(const Owner& owner,const StableId& id){return owner.kind==Owner::Kind::actor&&owner.id==id;}
// 仅显式Task2现金收据可证明过去已结；旧status/今天余额不能单独倒填历史免责。
bool settled_static_before(const CommonStore& c,const TaskState& tasks,const TaskRecord& task,const TaskDefinition& definition,Integer cutoff,Integer tick,const StableId& boundary){
    if(definition.template_id!="message"&&definition.template_id!="escort"&&definition.template_id!="procurement")return false;
    if(task.contract_id||!tasks.takeover_execution||tasks.takeover_execution->version!=2||(task.status!="success"&&task.status!="failed"&&task.status!="cancelled"))return false;
    const auto settlement=serial(task.settlement_id,"settlement",c.state().next_instance_id);const auto* escrow=c.wallet(task.budget_wallet_id);if(!settlement||*settlement>=cutoff||!escrow||!owner_equal(escrow->owner,Owner{Owner::Kind::task,task.id,{}})||escrow->balance!=0||!escrow->reserved.empty())return false;
    const TaskStaticPaymentSource* acceptance=nullptr;const TaskStaticPaymentSource* terminal=nullptr;
    const auto& order=c.state().facts.order();const auto end=boundary.empty()?order.end():std::find(order.begin(),order.end(),boundary);if(!boundary.empty()&&end==order.end())return false;
    for(const auto& row:tasks.takeover_execution->static_payments.rows())if(row.second.task_id==task.id){const auto& payment=row.second;const auto* head=c.state().facts.find(row.first);const auto id=serial(row.first,"task_static_payment",c.state().next_instance_id);const auto position=std::find(order.begin(),order.end(),row.first);if(row.first!=payment.fact_id||!head||head->domain!="tasks"||head->kind!="task_static_payment"||!id||*id>=*settlement||payment.tick<0||payment.tick>tick||position==order.end()||position>=end)return false;auto*& slot=payment.stage==TaskStaticPaymentStage::acceptance?acceptance:terminal;if(slot)return false;slot=&payment;}
    if(!acceptance||!terminal||acceptance->tick!=task.accepted_tick||terminal->tick<acceptance->tick)return false;
    const auto first=std::find(order.begin(),order.end(),acceptance->fact_id),last=std::find(order.begin(),order.end(),terminal->fact_id);if(first>=last)return false;
    if(terminal->stage==TaskStaticPaymentStage::report_success){if(task.status!="success"||task.reported_tick!=terminal->tick)return false;}else if(terminal->stage==TaskStaticPaymentStage::report_failure){if(task.status!="failed"||task.reported_tick!=terminal->tick)return false;}else if(terminal->stage==TaskStaticPaymentStage::cancellation){if(task.reported_tick||(task.status!="cancelled"&&task.status!="failed"))return false;}else return false;
    // 新物理凭据必须与同Task原Task2终结付款同刻同Action；保留过的Task货后来清空也不能倒填。
    if(definition.template_id=="procurement"||(definition.template_id=="message"&&terminal->stage==TaskStaticPaymentStage::cancellation)){
        if(!tasks.static_assets||tasks.static_assets->version!=1)return false;const TaskStaticAssetReceipt* assets=nullptr;
        for(const auto& row:tasks.static_assets->receipts.rows())if(row.second.task_id==task.id){if(assets)return false;assets=&row.second;}
        if(!assets||!assets->retained_task_goods.empty()||assets->tick!=terminal->tick||assets->source_action_id!=terminal->source_action_id||assets->stage!=(terminal->stage==TaskStaticPaymentStage::report_success?"procurement_delivery":"cancellation_return"))return false;
        const auto* head=c.state().facts.find(assets->fact_id);const auto at=std::find(order.begin(),order.end(),assets->fact_id);const auto id=serial(assets->fact_id,task_static_asset_fact_kind,c.state().next_instance_id);
        if(!head||head->domain!="tasks"||head->kind!=task_static_asset_fact_kind||!id||*id>=*settlement||at==order.end()||at<=last||at>=end)return false;
    }
    // 护送原算法不生成Task货；私信成功仍须原唯一实交，取消改由上述原返还来源证明。
    if(definition.template_id=="message"&&terminal->stage!=TaskStaticPaymentStage::cancellation){if(task.status!="success"||!tasks.message_handoffs)return false;const TaskMessageHandoff* delivery=nullptr;for(const auto& row:tasks.message_handoffs->receipts.rows())if(row.second.task_id==task.id){if(delivery)return false;delivery=&row.second;}if(!delivery||delivery->carrier_actor_id!=task.assignee||delivery->recipient_actor_id!=definition.goal.receiver_actor||delivery->item_id!=definition.goal.message_id||delivery->tick>terminal->tick)return false;const auto* head=c.state().facts.find(delivery->fact_id);const auto at=std::find(order.begin(),order.end(),delivery->fact_id);if(!head||head->kind!="task_message_handoff"||head->domain!="tasks"||at==order.end()||at>=last)return false;}
    for(const auto& row:tasks.actions.rows())if(row.second.task_id==task.id)return false;
    for(const auto& row:c.state().inventories.rows())for(const auto& lot:row.second.lots)if(lot.owner.kind==Owner::Kind::task&&lot.owner.id==task.id)return false;
    for(const auto& row:tasks.shipments.rows())if(row.second.cargo_owner.kind==Owner::Kind::task&&row.second.cargo_owner.id==task.id)return false;
    for(const auto& row:tasks.cargo_exposures.rows())if(row.second.task_id==task.id)return false;
    return true;
}
// full World验证为前提，实例顺序只证明Task建立在交割前，不把当前status当过去终态。
Status absence(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const StableId& actor,Integer cutoff,Integer tick,const StableId& boundary){
    const auto unknown=[&](const StableId& id){return common_failure("InheritanceResponsibilityHistoryUnavailable",id);};const auto* deceased=c.actor(actor);if(!deceased||deceased->alive)return unknown(actor);if(tasks.shop_orders){auto valid=validate_task_shop_order_actor_history(c,tasks,*tasks.shop_orders,actor,boundary,tick);if(!valid.ok)return valid;}
    const auto earlier=[&](const StableId& task)->std::optional<bool>{const auto parsed=serial(task,"task",c.state().next_instance_id);if(!parsed)return {};return *parsed<cutoff;};
    for(const auto& row:tasks.tasks.rows()){auto prior=earlier(row.first);if(!prior)return unknown(row.first);if(!*prior)continue;const auto& task=row.second;if(tasks.shop_orders&&tasks.shop_orders->by_task.count(task.id))continue;const auto* d=resolve_task_definition(tasks,catalog,task);if(!d||(d->issuer.kind!=Owner::Kind::actor&&d->issuer.kind!=Owner::Kind::organization))return unknown(row.first);if(d->issuer.kind==Owner::Kind::organization){const auto* wallet=c.wallet(d->funding_wallet);if(!wallet||wallet->owner.kind!=Owner::Kind::organization)return unknown(row.first);}if((task.assignee==actor||person(d->issuer,actor)||d->goal.receiver_actor==actor||d->goal.actor_id==actor)&&!settled_static_before(c,tasks,task,*d,cutoff,tick,boundary))return unknown(row.first);if(task.contract_id){const auto* contract=tasks.contracts.find(*task.contract_id);if(!contract)return unknown(row.first);const auto related=[&](const SubjectRef& s){return s.kind==SubjectKind::actor&&s.id==actor;};if(related(contract->assignee)||related(contract->frozen_terms.beneficiary)||(contract->executor_actor_id&&*contract->executor_actor_id==actor))return unknown(*task.contract_id);}}
    // 同期原Task货物不能被遗产身份吞掉；后建立的任务不倒改已完成交割的历史结论。
    for(const auto& row:c.state().inventories.rows())if(person(row.second.owner,actor))for(const auto& lot:row.second.lots)if(lot.owner.kind==Owner::Kind::task){auto prior=earlier(lot.owner.id);if(!prior||*prior)return unknown(lot.id);}
    const auto before=[&](const StableId& id){if(boundary.empty())return true;const auto& order=c.state().facts.order();const auto a=std::find(order.begin(),order.end(),id),b=std::find(order.begin(),order.end(),boundary);return a!=order.end()&&b!=order.end()&&a<b;};
    for(const auto& row:tasks.transfers.rows())if(before(row.first)){const auto& transfer=row.second;if(person(transfer.from_owner,actor)||person(transfer.to_owner,actor)||transfer.from_inventory_id==deceased->inventory_id||transfer.to_inventory_id==deceased->inventory_id)return unknown(row.first);}
    if(tasks.personal_advances)for(const auto& row:tasks.personal_advances->terms.rows()){auto prior=earlier(row.second.task_id);if(!prior)return unknown(row.first);if(*prior&&(row.second.lender_actor_id==actor||person(row.second.original_issuer,actor)))return unknown(row.first);}
    for(const auto& row:tasks.shipments.rows()){const auto& shipment=row.second;auto prior=shipment.cargo_owner.kind==Owner::Kind::task?earlier(shipment.cargo_owner.id):std::optional<bool>{};if(!prior)return unknown(row.first);if(!*prior)continue;if((shipment.carrier_ref.kind==SubjectKind::actor&&shipment.carrier_ref.id==actor)||shipment.source_inventory_id==deceased->inventory_id||shipment.cargo_inventory_id==deceased->inventory_id||shipment.destination_inventory_id==deceased->inventory_id)return unknown(row.first);}
    return common_success();
}
}
Status validate_task_inheritance_history_provable_now(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const StableId& actor){return absence(c,tasks,catalog,actor,c.state().next_instance_id,c.tick(),{});}
Status validate_task_inheritance_history_before_fact(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const StableId& actor,const StableId& fact,Integer tick){const auto* header=c.state().facts.find(fact);const auto order=serial(fact,"family_inheritance",c.state().next_instance_id);if(!header||header->id!=fact||header->kind!="family_inheritance_settled"||header->domain!="family_inheritance"||!order||tick<0||tick>c.tick())return common_failure("InheritanceResponsibilityHistoryUnavailable",fact);return absence(c,tasks,catalog,actor,*order,tick,fact);}
}
