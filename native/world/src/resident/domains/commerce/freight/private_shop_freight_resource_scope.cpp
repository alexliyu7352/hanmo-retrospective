// 完整原recipe逐操作扫描，保留不增revision、间接释放和先减后补的触点；不从最终快照猜连续史。
#include "private_shop_freight_resource_scope.hpp"
#include <set>
#include <type_traits>
namespace hanmo::resident {namespace {
template<class>constexpr bool known_operation=false;
// 对原批的保守触点，按原item筛数量操作，但不复制FIFO或用今天Owner排除同recipe后续变化。
template<class T>bool touches(const T& op,const StableId& inventory,const LotView& lot){
    if constexpr(std::is_same_v<T,WalletDelta>||std::is_same_v<T,SetReservation>||std::is_same_v<T,CreateWallet>||std::is_same_v<T,SetWalletOwner>)return false;
    else if constexpr(std::is_same_v<T,ReleaseActionResources>||std::is_same_v<T,ReleaseReservationResources>)return true;
    else if constexpr(std::is_same_v<T,CreateInventory>){if(op.inventory.id==inventory)return true;return std::any_of(op.inventory.lots.begin(),op.inventory.lots.end(),[&](const LotView& x){return x.id==lot.id;});}
    else if constexpr(std::is_same_v<T,MoveIntegrationLot>)return op.lot_id==lot.id||op.destination_id==inventory;
    else if constexpr(std::is_same_v<T,TransferSelectedLots>)return (op.source_id==inventory&&std::any_of(op.selections.begin(),op.selections.end(),[&](const CargoSelection& s){return s.inventory_id==inventory&&s.lot_id==lot.id;}))||op.destination_id==inventory;
    else if constexpr(std::is_same_v<T,ReserveSelectedLots>||std::is_same_v<T,ConsumeSelectedLots>)return op.inventory_id==inventory&&std::any_of(op.selections.begin(),op.selections.end(),[&](const CargoSelection& s){return s.inventory_id==inventory&&s.lot_id==lot.id;});
    else if constexpr(std::is_same_v<T,TransferLots>)return (op.source_id==inventory&&op.item_id==lot.item_id)||op.destination_id==inventory;
    else if constexpr(std::is_same_v<T,AdjustLots>)return op.inventory_id==inventory&&std::any_of(op.lots.begin(),op.lots.end(),[&](const LotDelta& d){return d.lot_id==lot.id;});
    else if constexpr(std::is_same_v<T,SetLotOwner>)return op.inventory_id==inventory&&op.lot_id==lot.id;
    else if constexpr(std::is_same_v<T,ReserveLots>||std::is_same_v<T,ConsumeLots>)return op.inventory_id==inventory&&op.item_id==lot.item_id;
    else if constexpr(std::is_same_v<T,AddLot>||std::is_same_v<T,SetInventoryOwner>||std::is_same_v<T,SetInventoryCapacity>)return op.inventory_id==inventory;
    else static_assert(known_operation<T>,"New Common resource primitive requires freight scope review");
}
}
Outcome<std::vector<PrivateShopFreightResourceTouch>> inspect_private_shop_freight_resource_scope(const OrderedTable<InventoryView>& before,const std::vector<PrivateShopFreightCargoSelection>& selected,const ResourceRecipe& recipe){
    using Result=Outcome<std::vector<PrivateShopFreightResourceTouch>>;
    const auto fail=[](const StableId& code,const StableId& id)->Result{return {false,{},common_failure(code,id).error};};
    std::set<StableId> inventories,lot_ids;
    for(const auto& scope:selected){
        const auto* inventory=before.find(scope.inventory_id);
        if(scope.inventory_id.empty()||scope.lot_ids.empty()||!inventories.insert(scope.inventory_id).second||!inventory||inventory->id!=scope.inventory_id)return fail("PrivateShopFreightCargoSelectionInvalid",scope.inventory_id);
        for(const auto& id:scope.lot_ids){
            if(id.empty()||!lot_ids.insert(id).second)return fail("PrivateShopFreightCargoSelectionInvalid",id);
            const auto found=std::find_if(inventory->lots.begin(),inventory->lots.end(),[&](const LotView& lot){return lot.id==id;});
            if(found==inventory->lots.end()||found->item_id.empty()||found->quantity<=0)return fail("PrivateShopFreightCargoLotUnavailable",id);
            std::size_t occurrences=0;for(const auto& row:before.rows())for(const auto& lot:row.second.lots)if(lot.id==id)++occurrences;
            if(occurrences!=1)return fail("PrivateShopFreightCargoLotAmbiguous",id);
        }
    }
    std::vector<PrivateShopFreightResourceTouch> out;
    for(std::size_t index=0;index<recipe.operations.size();++index){
        PrivateShopFreightResourceTouch hit;hit.operation_index=index;
        for(const auto& scope:selected){
            const auto& inventory=*before.find(scope.inventory_id);PrivateShopFreightCargoSelection matching{scope.inventory_id,{}};
            for(const auto& id:scope.lot_ids){const auto& lot=*std::find_if(inventory.lots.begin(),inventory.lots.end(),[&](const LotView& value){return value.id==id;});if(std::visit([&](const auto& op){return touches(op,scope.inventory_id,lot);},recipe.operations[index]))matching.lot_ids.push_back(id);}
            if(!matching.lot_ids.empty())hit.selections.push_back(std::move(matching));
        }
        if(!hit.selections.empty())out.push_back(std::move(hit));
    }
    return {true,std::move(out),{}};
}
}
