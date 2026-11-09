// 真实授权回归：静态采购宽限结束后不能新花受托经费，检查本身不退款、不变更任务。
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/model/procurement_timing.hpp"
#ifndef HANMO_TEST_LEGACY_TASK_GATE
#include "resident/domains/tasks/task_execution_gate.hpp"
#endif
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
// 原实现分支只用于保存缺陷红门；默认测试必须调用修正后的正式入口。
bool authorized(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog){
#ifdef HANMO_TEST_LEGACY_TASK_GATE
    (void)common;return task_procurement_authorized(tasks,catalog,"player","task","grain");
#else
    return task_procurement_authorized_now(common,tasks,catalog,"player","task","grain");
#endif
}
// 付款源列表与真正授权使用同一时效门，不以UI隐藏代替规则拒绝。
std::vector<std::pair<StableId,StableId>> payers(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog){
#ifdef HANMO_TEST_LEGACY_TASK_GATE
    (void)common;return task_procurement_payers(tasks,catalog,"player");
#else
    return task_procurement_payers_now(common,tasks,catalog,"player");
#endif
}
// 截止、最后宽限、过期与动态明确停止分别检查，不引入新数值政策。
int main(){
    auto shared=std::make_shared<CommonCatalog>();shared->items.insert("grain",{"grain",1,10});RoleDefinition role;role.id="merchant";role.initial_actor="merchant";role.place_id="market";role.services={"trade","accept_offer","report_offer"};shared->roles.insert(role.id,role);
    CommonState seed;seed.identity.tick=10;seed.identity.revision=4;seed.next_instance_id=12;seed.wallets.insert("budget:task",{"budget:task",{Owner::Kind::task,"task",{}},50,{}});
    for(auto id:{"player","merchant"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=100;actor.wallet_id="cash:"+actor.id;actor.inventory_id="bag:"+actor.id;actor.location=SettlementLocation{"town","market"};seed.actors.insert(actor.id,actor);seed.wallets.insert(actor.wallet_id,{actor.wallet_id,{Owner::Kind::actor,id,{}},100,{}});seed.inventories.insert(actor.inventory_id,{actor.inventory_id,{Owner::Kind::actor,id,{}},100,0,{}});}
    seed.inventories.find("bag:merchant")->lots.push_back({"stock","grain",{},5,{Owner::Kind::actor,"merchant",{}}});seed.inventories.find("bag:player")->lots.push_back({"held-task-grain","grain",{},1,{Owner::Kind::task,"task",{}}});CommonStore common(seed,shared);
    TaskCatalog catalog;TaskDefinition offer;offer.id="offer";offer.template_id="procurement";offer.goal.item_id="grain";offer.goal.quantity=1;offer.goal.receiver_role="merchant";offer.accept_role="merchant";offer.report_role="merchant";offer.issuer={Owner::Kind::actor,"merchant",{}};offer.funding_wallet="cash:merchant";offer.reward=8;catalog.offers.insert(offer.id,offer);
    TaskState tasks;TaskRecord task;task.id="task";task.offer_id="offer";task.assignee="player";task.status="active";task.deadline_tick=10;task.budget_wallet_id="budget:task";tasks.tasks.insert(task.id,task);
    assert(authorized(common,tasks,catalog)&&payers(common,tasks,catalog).size()==1);
    common.state().identity.tick=11;assert(!authorized(common,tasks,catalog)&&payers(common,tasks,catalog).empty());
    common.state().identity.tick=12;assert(!authorized(common,tasks,catalog)&&payers(common,tasks,catalog).empty());
    common.state().identity.tick=13;const auto expired_authorized=authorized(common,tasks,catalog);const auto expired_payers=payers(common,tasks,catalog);
    std::cerr<<"task_expiry_probe: tick=13 deadline=10 authorized="<<expired_authorized<<" payers="<<expired_payers.size()<<" budget="<<common.wallet("budget:task")->balance<<"\n";
    assert(!expired_authorized&&expired_payers.empty());
    assert(common.wallet("budget:task")->balance==50&&common.state().next_instance_id==12&&common.revision()==4&&tasks.tasks.find("task")->status=="active"&&tasks.tasks.find("task")->settlement_id.empty());
    // 真实trade报价核共同耗时，拒绝公款采购时不影响私人买卖或已经持货的原回报。
    InteractionState interaction;InteractionCatalog interactions;interactions.common=shared;InteractionAccess ia;ia.resolve_location=[&](const StableId& id){return common.actor(id)->location;};ia.battle_occupied=[](const auto&){return false;};ia.role=[](const auto&){return std::optional<RoleAssignmentView>{{"merchant",true}};};
    GrowthCatalog shops;shops.shops.insert("shop",{"shop","market","bag:merchant","cash:merchant",{"merchant"}});shops.tradeable_items={"grain"};GrowthAccess access;access.contact=[&](const StableId& actor,const InteractionContext& context,const StableId& service,const StableId& own){return validate_interaction(common,interaction,interactions,ia,actor,context,service,own);};access.procurement_authorized=[&](const auto&,const auto&,const auto&){return authorized(common,tasks,catalog);};
    GrowthCommand purchase;purchase.kind="trade";purchase.shop_id="shop";purchase.item_id="grain";purchase.quantity=1;purchase.side="buy";purchase.payer_wallet_id="budget:task";purchase.context=InteractionContext{"market","merchant","merchant"};
    common.state().identity.tick=10;auto trade= growth::preview_service_trade(common,shops,*shared,access,"player",purchase,{});assert(trade.ok&&trade.value.duration==trade_action_ticks);
    common.state().identity.tick=11;trade=growth::preview_service_trade(common,shops,*shared,access,"player",purchase,{});assert(!trade.ok&&trade.error.original.code=="PublicGoodsRestricted");purchase.payer_wallet_id="cash:player";trade=growth::preview_service_trade(common,shops,*shared,access,"player",purchase,{});assert(trade.ok&&trade.value.duration==trade_action_ticks);
    TaskCommand report{"report_offer","offer",{"market","merchant","merchant"}};auto handover=preview_task(common,tasks,catalog,*shared,interaction,interactions,ia,"player",report);assert(handover.ok&&handover.value.duration==task_handover_ticks);common.state().identity.tick=12;handover=preview_task(common,tasks,catalog,*shared,interaction,interactions,ia,"player",report);assert(!handover.ok&&handover.error.original.code=="OfferExpired");
    assert(common.wallet("budget:task")->balance==50&&common.wallet("cash:player")->balance==100&&common.inventory("bag:player")->lots.front().quantity==1&&common.inventory("bag:merchant")->lots.front().quantity==5);
    std::cout<<"task_timing_quotes: actual_private_trade_duration="<<trade.value.duration<<" existing_goods_report_at_tick12="<<handover.ok<<" task_budget="<<common.wallet("budget:task")->balance<<" private_cash="<<common.wallet("cash:player")->balance<<" carried="<<common.inventory("bag:player")->lots.front().quantity<<" stock="<<common.inventory("bag:merchant")->lots.front().quantity<<"\n";
    // 动态合同仍依据原execution_stopped关闭新支出，不套静态两半日宽限。
    ContractRecord contract;contract.id="contract";contract.frozen_terms.definition=offer;contract.frozen_terms.deadline_tick=10;tasks.contracts.insert(contract.id,contract);tasks.tasks.find("task")->contract_id=contract.id;common.state().identity.tick=10;
    assert(authorized(common,tasks,catalog)&&payers(common,tasks,catalog).size()==1);
    tasks.contracts.find(contract.id)->execution_stopped=true;assert(!authorized(common,tasks,catalog)&&payers(common,tasks,catalog).empty());
    assert(common.wallet("budget:task")->balance==50&&common.revision()==4&&tasks.tasks.find("task")->settlement_id.empty());
    std::cout<<"task_execution_gate: deadline and grace boundaries, closed new spending, stable escrow and original dynamic stop authority passed\n";
}
