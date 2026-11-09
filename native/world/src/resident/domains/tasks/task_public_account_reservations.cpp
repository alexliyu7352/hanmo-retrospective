// 动态合同已经拨到Task托管，只有旧静态受领/报告会在原组织公库留下Action钱预约。
#include "task_public_account_reservations.hpp"
#include "task_public_account_phase.hpp"
#include "resident/domains/politics/takeover/politics_takeover_contract.hpp"
namespace hanmo::resident {
namespace {
Outcome<bool> bad(const StableId& id){return {false,{},common_failure("InvalidTaskPublicReservation",id).error};}
bool public_owner(const Owner& owner){return owner.kind==Owner::Kind::organization&&!owner.id.empty()&&owner_equal(owner,Owner{Owner::Kind::organization,owner.id,{}});}
// 第二次占领只沿前一次已apply的同Task登记保留原预约；未裁定不等于准许继续支出。
Outcome<bool> inherited_owner(const CommonStore& common,const TaskState& tasks,const TaskDefinition& definition,const TaskRecord& task,const Owner& current,const StableId& wallet_id,const PoliticalTakeoverRead* authority,Integer observed){
    if(owner_equal(definition.issuer,current))return {true,true,{}};
    if(!public_owner(definition.issuer)||!authority||!tasks.takeover_execution||(tasks.takeover_execution->version!=1&&tasks.takeover_execution->version!=2))return bad(task.id);
    auto source=authority->find_political_responsibility_for_execution({PoliticalResponsibilityKind::task_contract,task.id,{},{}},common.state().identity);if(!source.ok)return {false,{},source.error};if(!source.value)return bad(task.id);
    const auto& value=*source.value;const auto& registration=value.registration;const auto& identity=common.state().identity;const auto& actual=registration.source_identity;
    const auto* occupation=common.state().facts.find(registration.occupation_fact_id);const auto* header=common.state().facts.find(registration.registration_fact_id);
    if(value.responsibility.kind!=PoliticalResponsibilityKind::task_contract||value.responsibility.responsibility_id!=task.id||value.responsibility.unit_id||value.responsibility.asset_id||value.registration_fact_id.empty()||value.registration_fact_id!=registration.registration_fact_id||!occupation||occupation->domain!="military"||occupation->kind!="military_occupation"||!header||header->domain!="politics"||header->kind!="political_public_responsibilities_pending"||actual.world_id!=identity.world_id||actual.execution_epoch!=identity.execution_epoch||actual.revision!=identity.revision||actual.tick!=identity.tick||registration.occupation_tick<0||registration.occupation_tick>observed||registration.former_organization_id.empty()||registration.former_organization_id==registration.successor_organization_id||registration.successor_organization_id!=current.id||std::count(registration.public_wallet_ids.begin(),registration.public_wallet_ids.end(),wallet_id)!=1)return bad(task.id);
    if(value.status==PoliticalResponsibilityExecutionStatus::pending){if(value.decision)return bad(task.id);}
    else if(value.status==PoliticalResponsibilityExecutionStatus::approved||value.status==PoliticalResponsibilityExecutionStatus::suspended){if(!value.decision)return bad(task.id);}
    else return bad(task.id);
    return {true,true,{}};
}
}
Outcome<bool> read_task_public_wallet_reservation(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const Owner& owner,const StableId& wallet_id,const Reservation& reservation,const PoliticalTakeoverRead* authority,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    const auto* wallet=common.wallet(wallet_id);if(!public_owner(owner)||!wallet||!owner_equal(wallet->owner,owner)||reservation.action_id.empty()||reservation.amount<0)return bad(wallet_id);
    Integer matches=0;for(const auto& actual:wallet->reserved)if(actual.action_id==reservation.action_id){if(actual.amount!=reservation.amount)return bad(wallet_id);++matches;}if(matches!=1)return bad(wallet_id);
    const auto& ref=reservation.action_id;
    if(ref.kind!=ReservationRef::Kind::action){if(ref.kind==ReservationRef::Kind::contract&&tasks.contracts.find(ref.id))return bad(ref.id);return {true,false,{}};}
    const auto* params=tasks.actions.find(ref.id);if(!params)return {true,false,{}};
    const auto* action=common.action(ref.id);const auto* task=tasks.tasks.find(params->task_id);const auto* actor=task?common.actor(task->assignee):nullptr;const auto* definition=task?resolve_task_definition(tasks,catalog,*task):nullptr;
    if(!action||!task||!actor||!definition||task->contract_id||(task->offer_ref&&task->offer_ref->kind==OfferRef::Kind::runtime)||task->id!=params->task_id||task->offer_id!=params->offer_id||task->offer_id!=definition->id||!task->settlement_id.empty()||definition->funding_wallet!=wallet_id||action->id!=ref.id||action->actor_id!=task->assignee||actor->action_id!=ref.id||action->remaining_ticks<=0||action->start_tick<0||action->start_tick>common.tick()||params->delivery_quantity||params->restitution||(params->source_kind&&*params->source_kind!=action->kind))return bad(ref.id);
    const auto* provider=common.actor(params->context.provider_actor_id);const auto* escrow=common.wallet(task->budget_wallet_id);
    if(!provider||provider->action_id!=ref.id||params->context.place_id.empty()||params->context.role_id.empty()||std::find(action->participants.begin(),action->participants.end(),actor->id)==action->participants.end()||std::find(action->participants.begin(),action->participants.end(),provider->id)==action->participants.end()||!escrow||!owner_equal(escrow->owner,Owner{Owner::Kind::task,task->id,{}}))return bad(ref.id);
    if(action->kind=="accept_offer"){
        if(task->status!="accepting"||action->remaining_ticks>definition->accept_ticks||params->reward||reservation.amount!=definition->budget||escrow->balance!=0||!escrow->reserved.empty()||params->context.role_id!=definition->accept_role)return bad(ref.id);
    }else if(action->kind=="report_offer"){
        if((task->status!="active"&&task->status!="ready")||action->remaining_ticks!=1||!params->reward||*params->reward<0||*params->reward>definition->reward||reservation.amount!=*params->reward||params->context.role_id!=definition->report_role)return bad(ref.id);
        if(definition->template_id=="procurement"){auto quantity=common.count(actor->inventory_id,definition->goal.item_id,{Owner{Owner::Kind::task,task->id,{}},Owner{Owner::Kind::actor,actor->id,{}}},ref);if(!quantity.ok||quantity.value!=definition->goal.quantity)return bad(ref.id);}
        else if(definition->template_id=="message"){if(task->status!="ready")return bad(ref.id);}
        else if(definition->template_id=="escort"){if(task->status!="ready"||!task->escort||(!task->escort->failure.empty()&&*params->reward!=0))return bad(ref.id);}
        else return bad(ref.id);
    }else return bad(ref.id);
    return inherited_owner(common,tasks,*definition,*task,owner,wallet_id,authority,observed.value);
}
Outcome<bool> read_task_public_lot_reservation(const CommonStore& common,const TaskState& tasks,const TaskCatalog&,const Owner& owner,const StableId& inventory_id,const StableId& lot_id,const ReservationRef& ref,const PoliticalTakeoverRead*,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    const auto* inventory=common.inventory(inventory_id);if(!public_owner(owner)||!inventory||!owner_equal(inventory->owner,owner)||ref.empty())return bad(inventory_id);
    const LotView* lot=nullptr;for(const auto& row:inventory->lots)if(row.id==lot_id){if(lot)return bad(lot_id);lot=&row;}
    if(!lot||lot->quantity<=0||lot->reserved_by!=ref||!owner_equal(lot->owner,owner))return bad(lot_id);
    // 真正Task预约只保护本人背包的Task/私人货，当前没有组织公开货的Task消费规则。
    if((ref.kind==ReservationRef::Kind::action&&tasks.actions.find(ref.id))||(ref.kind==ReservationRef::Kind::contract&&tasks.contracts.find(ref.id)))return bad(lot_id);
    return {true,false,{}};
}
}
