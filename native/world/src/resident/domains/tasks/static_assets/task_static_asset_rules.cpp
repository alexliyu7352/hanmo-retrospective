// 只观察原TransferLots的精确变化；同候选事实失败仍由原World保存点恢复任务与全部钱货。
#include "task_static_asset.hpp"
#include <limits>
namespace hanmo::resident { namespace {
// phase只准当前半日，不能借一个任意未来tick制造货物历史。
Outcome<Integer> observed_tick(const CommonStore& common,const DynamicStepInput* phase){if(!phase)return {true,common.tick(),{}};if(phase->from_tick!=common.tick()||phase->from_tick<0||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return {false,{},common_failure("TaskStaticAssetPhaseMismatch").error};return {true,phase->to_tick,{}};}
Owner task_owner(const StableId& id){return {Owner::Kind::task,id,{}};}
Owner actor_owner(const StableId& id){return {Owner::Kind::actor,id,{}};}
// 原批次附带字段不授新权；业务身份、产权、预约与准确数量必须相同。
bool same_lot(const LotView& a,const LotView& b){return a.id==b.id&&a.item_id==b.item_id&&a.quantity==b.quantity&&a.reserved_by==b.reserved_by&&owner_equal(a.owner,b.owner);}
bool same_lots(const std::vector<LotView>& a,const std::vector<LotView>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same_lot(a[i],b[i]))return false;return true;}
}
Outcome<TaskStaticAssetPreparation> prepare_task_static_asset_transfer(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const CommonCatalog& shared,const TaskStaticAssetState& state,const StableId& task_id,const StableId& stage,const ActionCoreView* action,Integer tick,const DynamicStepInput* phase){
    const auto fail=[](const StableId& code)->Outcome<TaskStaticAssetPreparation>{return {false,{},common_failure(code).error};};
    if(state.version!=1)return fail("UnsupportedTaskStaticAssetVersion");auto observed=observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};if(tick!=observed.value)return fail("TaskStaticAssetTickMismatch");
    const auto* task=tasks.tasks.find(task_id);const auto* definition=task?catalog.offers.find(task->offer_id):nullptr;
    if(!task||task->contract_id||!definition||(definition->template_id!="procurement"&&definition->template_id!="message"&&definition->template_id!="escort")||(task->status!="active"&&task->status!="ready")||!task->settlement_id.empty()||task->reported_tick||tick<task->accepted_tick)return fail("InvalidTaskStaticAssetTask");
    for(const auto& row:state.receipts.rows())if(row.second.task_id==task_id)return fail("DuplicateTaskStaticAssetTransfer");
    const auto* actor=common.actor(task->assignee);const auto* issuer=definition->issuer.kind==Owner::Kind::actor?common.actor(definition->issuer.id):nullptr;
    if(!actor||definition->issuer.extra||(definition->issuer.kind!=Owner::Kind::actor&&definition->issuer.kind!=Owner::Kind::organization)||(definition->issuer.kind==Owner::Kind::actor&&!issuer))return fail("InvalidTaskStaticAssetParties");
    const auto* source=common.inventory(actor->inventory_id);const auto* destination=common.inventory(issuer?issuer->inventory_id:"store:"+definition->issuer.id);
    if(!source||!destination||source->id==destination->id||!owner_equal(source->owner,actor_owner(actor->id))||!owner_equal(destination->owner,definition->issuer))return fail("InvalidTaskStaticAssetContainers");
    // 本版只认原issuer货物库；若已接管需另有具名历史货权口，不能借现金批准扩大收货权。
    TaskStaticAssetPreparation out;out.source_after=*source;out.destination_before=*destination;auto& receipt=out.receipt;
    receipt.task_id=task_id;receipt.offer_id=definition->id;receipt.assignee_actor_id=task->assignee;receipt.stage=stage;receipt.original_issuer=definition->issuer;receipt.from_inventory_id=source->id;receipt.to_inventory_id=destination->id;receipt.from_container_owner=source->owner;receipt.to_container_owner=destination->owner;receipt.tick=tick;
    // 选择序与原TransferLots一致：先产权顺序，再原容器批次顺序；只计算，不搬货也不分配ID。
    const auto select=[&](const StableId& item,Integer quantity,const std::vector<Owner>& owners,const ReservationRef& reservation)->Status{
        if(quantity<0)return common_failure("InvalidTaskStaticAssetQuantity");Integer remaining=quantity;
        for(const auto& owner:owners){for(auto& lot:out.source_after.lots){if(!remaining)break;if(lot.item_id!=item||!owner_equal(lot.owner,owner)||lot.reserved_by!=reservation)continue;if(lot.quantity<=0)return common_failure("InvalidTaskStaticAssetLot");const Integer used=std::min(remaining,lot.quantity);receipt.legs.push_back({item,lot.id,{},owner,definition->issuer,reservation,used,lot.quantity,lot.quantity-used});lot.quantity-=used;remaining-=used;}if(!remaining)break;}
        if(remaining)return common_failure("InsufficientGoods");return common_success();
    };
    if(stage=="procurement_delivery"){
        const auto* live=action?common.action(action->id):nullptr;const auto* params=action?tasks.actions.find(action->id):nullptr;
        if(definition->template_id!="procurement"||definition->goal.quantity<=0||!live||!params||params->task_id!=task_id||params->offer_id!=task->offer_id||live->kind!="report_offer"||action->kind!=live->kind||live->actor_id!=task->assignee||action->actor_id!=live->actor_id||params->context.role_id!=definition->report_role||params->context.role_id!=definition->goal.receiver_role||actor->action_id!=live->id)return fail("InvalidTaskStaticAssetAction");
        auto reserved=common.count(source->id,definition->goal.item_id,{task_owner(task_id),actor_owner(task->assignee)},ReservationRef{action->id});if(!reserved.ok||reserved.value!=definition->goal.quantity)return fail("TaskStaticAssetReservationMismatch");receipt.source_action_id=action->id;auto selected=select(definition->goal.item_id,definition->goal.quantity,{task_owner(task_id),actor_owner(task->assignee)},ReservationRef{action->id});if(!selected.ok)return {false,{},selected.error};
    }else if(stage=="cancellation_return"){
        if(action||!actor->action_id.empty())return fail("InvalidTaskStaticAssetCancellation");
        for(const auto& row:tasks.actions.rows())if(row.second.task_id==task_id)return fail("InvalidTaskStaticAssetCancellation");
        for(const auto& item:shared.items.rows()){auto amount=common.count(source->id,item.first,{task_owner(task_id)});if(!amount.ok)return {false,{},amount.error};auto selected=select(item.first,amount.value,{task_owner(task_id)},{});if(!selected.ok)return {false,{},selected.error};}
    }else return fail("InvalidTaskStaticAssetStage");
    if(action){for(auto& lot:out.source_after.lots)if(lot.reserved_by==ReservationRef{action->id})lot.reserved_by.clear();for(auto& lot:out.destination_before.lots)if(lot.reserved_by==ReservationRef{action->id})lot.reserved_by.clear();}
    auto& after=out.source_after.lots;after.erase(std::remove_if(after.begin(),after.end(),[](const LotView& row){return row.quantity<=0;}),after.end());
    return {true,std::move(out),{}};
}
Status record_task_static_asset_transfer_after_resources(CommonStore& common,TaskStaticAssetState& state,const TaskStaticAssetPreparation& prepared){
    const auto& expected=prepared.receipt;if(state.version!=1||!expected.fact_id.empty()||!expected.retained_task_goods.empty())return common_failure("InvalidTaskStaticAssetPreparation");
    for(const auto& row:state.receipts.rows())if(row.second.task_id==expected.task_id)return common_failure("DuplicateTaskStaticAssetTransfer",expected.task_id);
    const auto* source=common.inventory(expected.from_inventory_id);const auto* destination=common.inventory(expected.to_inventory_id);
    if(!source||!destination||!owner_equal(source->owner,expected.from_container_owner)||!owner_equal(destination->owner,expected.to_container_owner)||!same_lots(source->lots,prepared.source_after.lots)||destination->lots.size()!=prepared.destination_before.lots.size()+expected.legs.size())return common_failure("TaskStaticAssetResultMismatch");
    for(std::size_t i=0;i<prepared.destination_before.lots.size();++i)if(!same_lot(destination->lots[i],prepared.destination_before.lots[i]))return common_failure("TaskStaticAssetExistingLotChanged");
    auto receipt=expected;
    for(std::size_t i=0;i<receipt.legs.size();++i){auto& leg=receipt.legs[i];const auto& actual=destination->lots[prepared.destination_before.lots.size()+i];if(!leg.destination_lot_id.empty()||actual.id==leg.source_lot_id||actual.item_id!=leg.item_id||actual.quantity!=leg.quantity||actual.reserved_by!=ReservationRef{}||!owner_equal(actual.owner,leg.destination_owner))return common_failure("TaskStaticAssetTransferredLotMismatch");leg.destination_lot_id=actual.id;}
    for(const auto& row:common.state().inventories.rows())for(const auto& lot:row.second.lots)if(owner_equal(lot.owner,task_owner(receipt.task_id)))receipt.retained_task_goods.push_back({row.first,lot.id,lot.item_id,lot.reserved_by,lot.quantity});
    auto id=common.next_id(task_static_asset_fact_kind);if(!id.ok)return {false,{},id.error};receipt.fact_id=id.value;
    auto appended=common.append_fact({receipt.fact_id,task_static_asset_fact_kind,"tasks"});if(!appended.ok)return appended;
    if(!state.receipts.insert(id.value,std::move(receipt)))return common_failure("DuplicateTaskStaticAssetTransfer",id.value);state.observed_by[expected.assignee_actor_id].push_back(id.value);return common_success();
}
}
