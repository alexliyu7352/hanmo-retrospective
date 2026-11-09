// 不扫描全体本人的任务当店债：只匹配精确钱包/库存ID；不可证旧历史保持显式失败。
#include "task_shop_asset_responsibilities.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.rfind(head,0)!=0)return {};auto text=id.substr(head.size());Integer n=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||n<=0||n>=next||std::to_string(n)!=text)return {};return n;}
Outcome<TaskPrivateShopAssetResponsibilities> read(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const TaskShopOrderState* orders,const TaskPrivateShopAssetScope& scope,const StableId& closing,Integer tick,Integer cutoff){
    const auto unknown=[](const StableId& id)->Outcome<TaskPrivateShopAssetResponsibilities>{return {false,{},common_failure("TaskShopAssetResponsibilityHistoryUnavailable",id).error};};
    const auto* wallet=c.wallet(scope.wallet_id);const auto* inventory=c.inventory(scope.inventory_id);const Owner proprietor{Owner::Kind::actor,scope.proprietor_actor_id,{}};
    if(scope.shop_id.empty()||!c.actor(scope.proprietor_actor_id)||!wallet||!inventory||!owner_equal(wallet->owner,proprietor)||!owner_equal(inventory->owner,proprietor))return unknown(scope.shop_id);
    if(orders){auto valid=validate_task_shop_order_ledger(c,tasks,*orders);if(!valid.ok)return {false,{},valid.error};}else for(const auto& fact:c.state().facts.rows())if(fact.second.domain==task_shop_order_domain||fact.second.kind==task_shop_order_acceptance_kind||fact.second.kind==task_shop_order_settled_kind||fact.second.kind==task_shop_order_interrupted_kind)return unknown(fact.first);
    TaskPrivateShopAssetResponsibilities result{c.state().identity,scope,tick,closing,{}};std::set<StableId> emitted,shop_tasks;
    const auto pending=[&](const StableId& id){if(emitted.insert(id).second)result.pending_source_ids.push_back(id);};
    const auto relevant=[&](const StableId& wallet_id,const StableId& inventory_id){return (!wallet_id.empty()&&wallet_id==scope.wallet_id)||(!inventory_id.empty()&&inventory_id==scope.inventory_id);};
    const auto& facts=c.state().facts.order();const auto end=closing.empty()?facts.end():std::find(facts.begin(),facts.end(),closing);
    if(orders)for(const auto& row:orders->bindings.rows()){
        const auto& b=row.second;shop_tasks.insert(b.task_id);const auto& t=b.terms;if(!relevant(t.shop_wallet_id,t.stock_inventory_id)&&!relevant(t.client_wallet_id,t.client_inventory_id))continue;
        // 店主这两原容器必须仍属于当前所问原店；任意别的店ID不得借空集合通过。
        if((t.shop_wallet_id==scope.wallet_id||t.stock_inventory_id==scope.inventory_id)&&(t.shop_id!=scope.shop_id||t.proprietor_actor_id!=scope.proprietor_actor_id))return unknown(row.first);
        const auto accepted=serial(b.fact_id,task_shop_order_acceptance_prefix,c.state().next_instance_id);const auto acceptance_position=std::find(facts.begin(),facts.end(),b.fact_id);if(!accepted||acceptance_position==facts.end()||((*accepted<cutoff)!=(acceptance_position<end))||(*accepted<cutoff?t.accepted_tick>tick:t.accepted_tick<tick))return unknown(row.first);if(*accepted>=cutoff)continue;
        const TaskShopOrderSettlement* receipt=nullptr;for(const auto& settled:orders->settlements.rows())if(settled.second.task_id==b.task_id)receipt=&settled.second;
        if(!receipt){pending(b.task_id);continue;}const auto n=serial(receipt->fact_id,task_shop_order_settled_kind,c.state().next_instance_id),s=serial(receipt->settlement_id,"settlement",c.state().next_instance_id);const auto position=std::find(facts.begin(),facts.end(),receipt->fact_id);
        if(!n||!s||*n>=cutoff||*s>=cutoff||receipt->tick>tick||position==facts.end()||position>=end)pending(b.task_id);
        // 新执行人取消不能仅用较早的原结算免责；真实执行来源也须在该实际关店前。
        if(receipt->termination==task_shop_order_estate_termination){const TaskShopOrderEstateCancellation* executor=nullptr;if(orders->estate_cancellations)for(const auto& row:orders->estate_cancellations->cancellations.rows())if(row.second.settlement_fact_id==receipt->fact_id)executor=&row.second;if(!executor)return unknown(receipt->fact_id);const auto source=serial(executor->fact_id,task_shop_order_estate_cancelled_kind,c.state().next_instance_id);const auto at=std::find(facts.begin(),facts.end(),executor->fact_id);if(!source||at==facts.end()||((*source<cutoff)!=(at<end))||(*source<cutoff?executor->tick>tick:executor->tick<tick))return unknown(executor->fact_id);if(*source>=cutoff)pending(b.task_id);}

    }
    // 旧源只要实际引用选定容器便要求以后补专门历史证明；零余额、今天无预约、自由status均无免责作用。
    for(const auto& row:tasks.tasks.rows()){
        const auto& task=row.second;if(shop_tasks.count(task.id))continue;const auto id=serial(task.id,"task",c.state().next_instance_id);if(!id)return unknown(row.first);if(*id>=cutoff)continue;
        if(relevant(task.budget_wallet_id,{}))return unknown(row.first);const auto* assignee=c.actor(task.assignee);if(assignee&&relevant(assignee->wallet_id,assignee->inventory_id))return unknown(row.first);const auto* d=resolve_task_definition(tasks,catalog,task);
        if(d){if(relevant(d->funding_wallet,{}))return unknown(row.first);for(const auto& actor_id:{task.assignee,d->issuer.kind==Owner::Kind::actor?d->issuer.id:StableId{},d->goal.receiver_actor,d->goal.actor_id}){const auto* actor=c.actor(actor_id);if(actor&&relevant(actor->wallet_id,actor->inventory_id))return unknown(row.first);}}
        else if(task.offer_id.rfind("private_shop_order:",0)==0)return unknown(row.first);
    }
    for(const auto& row:tasks.contracts.rows()){const auto& t=row.second.frozen_terms;if(relevant(t.source_wallet_id,t.target_inventory_id)||relevant(t.reward_wallet_id,{})||relevant(t.payee_wallet_id,{}))return unknown(row.first);}
    for(const auto& row:tasks.actions.rows()){const auto* d=resolve_known_offer_definition(tasks,catalog,row.second.offer_id);if(d&&relevant(d->funding_wallet,{}))return unknown(row.first);}
    for(const auto& row:tasks.deliveries.rows())if(relevant({},row.second.from_inventory_id)||relevant({},row.second.to_inventory_id))return unknown(row.first);
    for(const auto& row:tasks.transfers.rows())if(relevant({},row.second.from_inventory_id)||relevant({},row.second.to_inventory_id))return unknown(row.first);
    for(const auto& row:tasks.shipments.rows())if(relevant({},row.second.source_inventory_id)||relevant({},row.second.cargo_inventory_id)||relevant({},row.second.destination_inventory_id))return unknown(row.first);
    for(const auto& row:tasks.cargo_exposures.rows())if(relevant({},row.second.source_inventory_id)||relevant({},row.second.destination_inventory_id))return unknown(row.first);
    for(const auto& row:tasks.cargo_dispositions.rows())if(relevant({},row.second.from_inventory_id)||relevant({},row.second.to_inventory_id))return unknown(row.first);
    if(tasks.personal_advances){for(const auto& row:tasks.personal_advances->terms.rows()){const auto& t=row.second;if(relevant(t.personal_wallet_id,{})||relevant(t.escrow_wallet_id,{})||relevant(t.repayment_wallet_id,{}))return unknown(row.first);}for(const auto& row:tasks.personal_advances->payments.rows())if(relevant(row.second.from_wallet_id,{})||relevant(row.second.to_wallet_id,{}))return unknown(row.first);for(const auto& row:tasks.personal_advances->repayments.rows())if(relevant(row.second.to_wallet_id,{})||relevant(row.second.escrow_wallet_id,{})||relevant(row.second.issuer_wallet_id,{}))return unknown(row.first);}
    if(tasks.takeover_execution){for(const auto& row:tasks.takeover_execution->payments.rows())for(const auto& leg:row.second.legs)if(relevant(leg.from_wallet_id,{})||relevant(leg.to_wallet_id,{}))return unknown(row.first);for(const auto& row:tasks.takeover_execution->static_payments.rows())for(const auto& leg:row.second.legs)if(relevant(leg.from_wallet_id,{})||relevant(leg.to_wallet_id,{}))return unknown(row.first);}
    if(tasks.static_assets)for(const auto& row:tasks.static_assets->receipts.rows()){if(relevant({},row.second.from_inventory_id)||relevant({},row.second.to_inventory_id))return unknown(row.first);for(const auto& lot:row.second.retained_task_goods)if(relevant({},lot.inventory_id))return unknown(row.first);}
    if(tasks.message_handoffs)for(const auto& row:tasks.message_handoffs->receipts.rows())if(relevant({},row.second.from_inventory_id)||relevant({},row.second.to_inventory_id))return unknown(row.first);
    if(tasks.message_notices)for(const auto& row:tasks.message_notices->bindings.rows())if(relevant({},row.second.carrier_inventory_id))return unknown(row.first);
    // 仍存在的Task货权/原预约是额外拒绝证据；历史空缺从不由现在已清空来补证。
    for(const auto& lot:inventory->lots)if(lot.owner.kind==Owner::Kind::task||(lot.reserved_by.kind==ReservationRef::Kind::action&&tasks.actions.find(lot.reserved_by.id))||(lot.reserved_by.kind==ReservationRef::Kind::contract&&tasks.contracts.find(lot.reserved_by.id)))pending(lot.id);
    for(const auto& reservation:wallet->reserved)if((reservation.action_id.kind==ReservationRef::Kind::action&&tasks.actions.find(reservation.action_id.id))||(reservation.action_id.kind==ReservationRef::Kind::contract&&tasks.contracts.find(reservation.action_id.id)))pending(reservation.action_id.id);
    return {true,std::move(result),{}};
}
}
Outcome<TaskPrivateShopAssetResponsibilities> read_task_private_shop_asset_responsibilities(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const TaskShopOrderState* orders,const TaskPrivateShopAssetScope& scope){return read(c,tasks,catalog,orders,scope,{},c.tick(),c.state().next_instance_id);}
Outcome<TaskPrivateShopAssetResponsibilities> read_task_private_shop_asset_responsibilities_before_close(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const TaskShopOrderState* orders,const TaskPrivateShopAssetScope& scope,const StableId& fact,Integer tick){const auto* header=c.state().facts.find(fact);const auto n=serial(fact,"private_shop_closed",c.state().next_instance_id);if(!header||header->id!=fact||header->kind!="private_shop_closed"||header->domain!="private_shops"||!n||tick<0||tick>c.tick())return {false,{},common_failure("TaskShopAssetResponsibilityHistoryUnavailable",fact).error};return read(c,tasks,catalog,orders,scope,fact,tick,*n);}
}
