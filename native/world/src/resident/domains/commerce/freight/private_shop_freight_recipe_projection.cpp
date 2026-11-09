// 只核原映射的局部守恒与同一recipe相邻值，不另跑FIFO或从最后净差生成来源。
#include "private_shop_freight_recipe_projection.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
using Key=std::pair<StableId,StableId>;
// 同一次临时执行中的元数据应保持实际拥有值；不接受另造等额批次替换源。
bool same_owner(const Owner& a,const Owner& b){return owner_equal(a,b)&&a.extra==b.extra&&a.field_order==b.field_order;}
bool same_frame(const PrivateShopFreightLotFrame& a,const PrivateShopFreightLotFrame& b){
    const auto& x=a.lot;const auto& y=b.lot;
    return a.inventory_id==b.inventory_id&&same_owner(a.inventory_owner,b.inventory_owner)&&a.capacity==b.capacity&&a.revision==b.revision&&x.id==y.id&&x.item_id==y.item_id&&x.quantity==y.quantity&&same_owner(x.owner,y.owner)&&x.reserved_by==y.reserved_by&&same_resource_lot_value(x,y);
}
// 单批ID在一个真实容器最多出现一次；空值由调用者区分合法耗尽与缺少原输入。
Outcome<std::optional<PrivateShopFreightLotFrame>> read_lot(const InventoryView& inventory,const StableId& id){
    std::optional<PrivateShopFreightLotFrame> found;
    for(const auto& lot:inventory.lots)if(lot.id==id){if(found||lot.item_id.empty()||lot.quantity<=0)return {false,{},common_failure("PrivateShopFreightRecipeLotAmbiguous",id).error};found=PrivateShopFreightLotFrame{inventory.id,inventory.owner,inventory.capacity,inventory.revision,lot};}
    return {true,std::move(found),{}};
}
// 正负映射金额是原数量，不以整数回绕掩盖缺批或新货混入。
bool add_quantity(std::map<Key,Integer>& values,const Key& key,Integer quantity){Integer next=0;if(__builtin_add_overflow(values[key],quantity,&next)||next<0)return false;values[key]=next;return true;}
// 只匹配原语自己的实际端点和类别；不重新选择FIFO批次，也不把另种原语伪装成转运。
bool matching_operation(const ResourceOperation& operation,const ResourceLotFlowDraft& flow){
    switch(flow.kind){
        case ResourceLotFlowDraft::Kind::reserve_whole:case ResourceLotFlowDraft::Kind::reserve_split:{const auto* p=std::get_if<ReserveLots>(&operation);return p&&p->inventory_id==flow.source_inventory_id&&p->inventory_id==flow.destination_inventory_id&&p->item_id==flow.item_id;}
        case ResourceLotFlowDraft::Kind::transfer:{const auto* p=std::get_if<TransferLots>(&operation);return p&&p->source_id==flow.source_inventory_id&&p->destination_id==flow.destination_inventory_id&&p->item_id==flow.item_id;}
        case ResourceLotFlowDraft::Kind::consume:{const auto* p=std::get_if<ConsumeLots>(&operation);return p&&p->inventory_id==flow.source_inventory_id&&p->item_id==flow.item_id;}
        case ResourceLotFlowDraft::Kind::adjust:{const auto* p=std::get_if<AdjustLots>(&operation);return p&&p->inventory_id==flow.source_inventory_id&&std::any_of(p->lots.begin(),p->lots.end(),[&](const auto& d){return d.lot_id==flow.source_lot_id&&d.delta==flow.quantity;});}
        case ResourceLotFlowDraft::Kind::integration_move:{const auto* p=std::get_if<MoveIntegrationLot>(&operation);return p&&p->source_id==flow.source_inventory_id&&p->destination_id==flow.destination_inventory_id&&p->lot_id==flow.source_lot_id&&p->lot_id==flow.destination_lot_id&&p->expected_quantity==flow.quantity;}
        case ResourceLotFlowDraft::Kind::selected_reserve_whole:case ResourceLotFlowDraft::Kind::selected_reserve_split:{const auto* p=std::get_if<ReserveSelectedLots>(&operation);return p&&p->inventory_id==flow.source_inventory_id&&p->inventory_id==flow.destination_inventory_id&&std::count_if(p->selections.begin(),p->selections.end(),[&](const CargoSelection& q){return q.inventory_id==flow.source_inventory_id&&q.lot_id==flow.source_lot_id&&q.quantity==flow.quantity;})==1;}
        case ResourceLotFlowDraft::Kind::selected_transfer_whole:case ResourceLotFlowDraft::Kind::selected_transfer_split:{const auto* p=std::get_if<TransferSelectedLots>(&operation);return p&&p->source_id==flow.source_inventory_id&&p->destination_id==flow.destination_inventory_id&&std::count_if(p->selections.begin(),p->selections.end(),[&](const CargoSelection& q){return q.inventory_id==flow.source_inventory_id&&q.lot_id==flow.source_lot_id&&q.quantity==flow.quantity;})==1;}
        case ResourceLotFlowDraft::Kind::selected_consume:{const auto* p=std::get_if<ConsumeSelectedLots>(&operation);return p&&p->inventory_id==flow.source_inventory_id&&std::count_if(p->selections.begin(),p->selections.end(),[&](const CargoSelection& q){return q.inventory_id==flow.source_inventory_id&&q.lot_id==flow.source_lot_id&&q.quantity==flow.quantity;})==1;}
        default:return false;
    }
}
}
Outcome<PrivateShopFreightRecipeProjection> project_private_shop_freight_recipe(const ResourceExecutionDraft& recipe,const std::vector<PrivateShopFreightCargoSelection>& selected){
    const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<PrivateShopFreightRecipeProjection>{return {false,{},common_failure(code,id).error};};
    std::vector<Key> live;std::set<StableId> inventories,ids;
    for(const auto& scope:selected){if(scope.inventory_id.empty()||scope.lot_ids.empty()||!inventories.insert(scope.inventory_id).second)return fail("PrivateShopFreightCargoSelectionInvalid",scope.inventory_id);for(const auto& id:scope.lot_ids){if(id.empty()||!ids.insert(id).second)return fail("PrivateShopFreightCargoSelectionInvalid",id);live.emplace_back(scope.inventory_id,id);}}
    if(recipe.next_instance_before<=0||recipe.next_instance_after<recipe.next_instance_before||recipe.operations.size()!=recipe.recipe.operations.size())return fail("PrivateShopFreightRecipeShapeMismatch");
    PrivateShopFreightRecipeProjection out;out.source_identity=recipe.identity;std::map<Key,PrivateShopFreightLotFrame> known;Integer next=recipe.next_instance_before;
    for(std::size_t index=0;index<recipe.operations.size();++index){
        const auto& op=recipe.operations[index];
        if(is_selected_resource_operation(recipe.recipe.operations[index])){auto checked=validate_selected_resource_execution(recipe.recipe.operations[index],op);if(!checked.ok)return {false,{},checked.error};}
        if(op.operation_index!=index||op.next_instance_before!=next||op.next_instance_after<next)return fail("PrivateShopFreightRecipeOperationOrder");next=op.next_instance_after;
        std::map<StableId,const ResourceInventoryExecutionDraft*> frames;
        for(const auto& frame:op.inventories)if(frame.inventory_id.empty()||frame.after.id!=frame.inventory_id||(frame.before&&frame.before->id!=frame.inventory_id)||!frames.emplace(frame.inventory_id,&frame).second)return fail("PrivateShopFreightRecipeInventoryMismatch",frame.inventory_id);
        for(const auto& frame:op.inventories)if(frame.before)for(const auto& lot:frame.before->lots)if(ids.count(lot.id)&&std::find(live.begin(),live.end(),Key{frame.inventory_id,lot.id})==live.end())return fail("PrivateShopFreightRecipeForeignLot",lot.id);
        PrivateShopFreightOperationProjection row;row.operation_index=index;row.original_operation=recipe.recipe.operations[index];
        std::map<Key,PrivateShopFreightLotFrame> initial;std::map<Key,Integer> quantities;std::map<Key,StableId> items;std::vector<Key> candidates=live;
        for(const auto& key:live){auto touched=frames.find(key.first);if(touched==frames.end())continue;const auto& frame=*touched->second;if(!frame.before)return fail("PrivateShopFreightRecipeOriginalLotRequired",key.second);auto original=read_lot(*frame.before,key.second);if(!original.ok)return {false,{},original.error};if(!original.value)return fail("PrivateShopFreightRecipeOriginalLotRequired",key.second);auto prior=known.find(key);if(prior!=known.end()&&!same_frame(prior->second,*original.value))return fail("PrivateShopFreightRecipeContinuityMismatch",key.second);initial.emplace(key,*original.value);quantities.emplace(key,original.value->lot.quantity);items.emplace(key,original.value->lot.item_id);row.before_lots.push_back(*original.value);}
        // 只以本原语开始时真实在场的源继承货批；同品新建或不明反向流入不能自动成为本批。
        for(const auto& flow:op.lot_flows){
            Key source{flow.source_inventory_id,flow.source_lot_id},destination{flow.destination_inventory_id,flow.destination_lot_id};auto original=initial.find(source);
            if(original==initial.end()){
                if(std::find(live.begin(),live.end(),destination)!=live.end()||std::find(live.begin(),live.end(),source)!=live.end())return fail("PrivateShopFreightRecipeForeignFlow",flow.source_lot_id);
                continue;
            }
            if(flow.item_id!=original->second.lot.item_id||!matching_operation(row.original_operation,flow))return fail("PrivateShopFreightRecipeItemMismatch",flow.source_lot_id);
            row.original_flows.push_back(flow);
            if(flow.kind==ResourceLotFlowDraft::Kind::adjust){if(destination!=source||!add_quantity(quantities,source,flow.quantity))return fail("PrivateShopFreightRecipeFlowMismatch",flow.source_lot_id);continue;}
            if(flow.quantity<=0)return fail("PrivateShopFreightRecipeFlowMismatch",flow.source_lot_id);
            if((flow.kind==ResourceLotFlowDraft::Kind::reserve_whole||flow.kind==ResourceLotFlowDraft::Kind::selected_reserve_whole)){if(destination!=source||flow.quantity!=original->second.lot.quantity)return fail("PrivateShopFreightRecipeFlowMismatch",flow.source_lot_id);continue;}
            if(!add_quantity(quantities,source,-flow.quantity))return fail("PrivateShopFreightRecipeQuantityOverflow",flow.source_lot_id);
            if((flow.kind==ResourceLotFlowDraft::Kind::consume||flow.kind==ResourceLotFlowDraft::Kind::selected_consume)){if(!destination.first.empty()||!destination.second.empty())return fail("PrivateShopFreightRecipeFlowMismatch",flow.source_lot_id);continue;}
            if(flow.kind!=ResourceLotFlowDraft::Kind::reserve_split&&flow.kind!=ResourceLotFlowDraft::Kind::transfer&&flow.kind!=ResourceLotFlowDraft::Kind::integration_move&&flow.kind!=ResourceLotFlowDraft::Kind::selected_transfer_whole&&flow.kind!=ResourceLotFlowDraft::Kind::selected_transfer_split&&flow.kind!=ResourceLotFlowDraft::Kind::selected_reserve_split)return fail("PrivateShopFreightRecipeFlowMismatch",flow.source_lot_id);
            if(destination.first.empty()||destination.second.empty()||destination==source||std::find(candidates.begin(),candidates.end(),destination)!=candidates.end()||!frames.count(destination.first))return fail("PrivateShopFreightRecipeDestinationMismatch",flow.destination_lot_id);
            if(flow.kind==ResourceLotFlowDraft::Kind::integration_move||flow.kind==ResourceLotFlowDraft::Kind::selected_transfer_whole){if(destination.second!=source.second||flow.quantity!=original->second.lot.quantity)return fail("PrivateShopFreightRecipeDestinationMismatch",flow.destination_lot_id);}else if(!ids.insert(destination.second).second)return fail("PrivateShopFreightRecipeDestinationMismatch",flow.destination_lot_id);
            const auto& target=*frames.at(destination.first);if(target.before){auto existing=read_lot(*target.before,destination.second);if(!existing.ok)return {false,{},existing.error};if(existing.value)return fail("PrivateShopFreightRecipeDestinationMismatch",flow.destination_lot_id);}
            candidates.push_back(destination);quantities.emplace(destination,flow.quantity);items.emplace(destination,flow.item_id);
        }
        for(const auto& frame:op.inventories)for(const auto& lot:frame.after.lots)if(ids.count(lot.id)&&std::find(candidates.begin(),candidates.end(),Key{frame.inventory_id,lot.id})==candidates.end())return fail("PrivateShopFreightRecipeForeignLot",lot.id);
        std::vector<Key> after;
        for(const auto& key:candidates){auto touched=frames.find(key.first);if(touched==frames.end()){after.push_back(key);continue;}auto actual=read_lot(touched->second->after,key.second);if(!actual.ok)return {false,{},actual.error};auto expected=quantities.find(key);if(expected==quantities.end()||expected->second<0||bool(actual.value)!=(expected->second>0)||(actual.value&&(actual.value->lot.quantity!=expected->second||actual.value->lot.item_id!=items.at(key))))return fail("PrivateShopFreightRecipeQuantityMismatch",key.second);if(actual.value){row.after_lots.push_back(*actual.value);known[key]=*actual.value;after.push_back(key);}else known.erase(key);}
        if(!row.before_lots.empty()||!row.after_lots.empty()||!row.original_flows.empty())out.operations.push_back(std::move(row));live=std::move(after);
    }
    if(next!=recipe.next_instance_after)return fail("PrivateShopFreightRecipeOperationOrder");
    for(const auto& key:live){auto scope=std::find_if(out.remaining_lots.begin(),out.remaining_lots.end(),[&](const auto& x){return x.inventory_id==key.first;});if(scope==out.remaining_lots.end())out.remaining_lots.push_back({key.first,{key.second}});else scope->lot_ids.push_back(key.second);}
    return {true,std::move(out),{}};
}
}
