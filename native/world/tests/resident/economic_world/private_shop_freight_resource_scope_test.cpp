// 只读触点组件：反例直接覆盖实际Common ResourceOperation，不冒已捕获永久货批连续史。
#include "resident/domains/commerce/freight/private_shop_freight_resource_scope.hpp"
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 所有检查均计数，源表自身的缺批/重号错误不能当作无触点。
void check(bool value,const char* message){++checks;if(!value){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
// 只保选中真实批次的有限初态，未选同品批不被静默纳入携运授权。
struct Fixture {
    OrderedTable<InventoryView> inventories;
    std::vector<PrivateShopFreightCargoSelection> selected{{"bag",{"lot:2"}}};
    Fixture(){InventoryView bag;bag.id="bag";bag.owner={Owner::Kind::actor,"carrier",{}};bag.capacity=100;bag.lots={{"lot:1","grain",{},3,bag.owner,{}},{"lot:2","grain",{},5,bag.owner,{}}};inventories.insert(bag.id,bag);}
    Outcome<std::vector<PrivateShopFreightResourceTouch>> inspect(std::vector<ResourceOperation> ops)const{return inspect_private_shop_freight_resource_scope(inventories,selected,{std::move(ops),{}});}
};
// 明确所有当前primitive类型；后续Common加新型时生产visitor须编译拒绝而不能默认跳过。
void primitive_coverage(){
    Fixture f;const Owner owner{Owner::Kind::actor,"carrier",{}};
    std::vector<ResourceOperation> ops={WalletDelta{"wallet",-7,true},SetReservation{"wallet",{"action:8"},7,false},CreateWallet{},SetWalletOwner{"wallet",owner},
        AddLot{"bag","other",1,owner,{}},ReserveLots{"bag","grain",1,{owner},{"action:8"}},TransferLots{"bag","other","grain",1,{owner},{},{}},ConsumeLots{"bag","grain",1,{owner},{}},
        ReleaseActionResources{"action:8"},ReleaseReservationResources{{ReservationRef::Kind::contract,"contract:8"}},SetInventoryCapacity{"bag",90},SetInventoryOwner{"bag",owner},SetLotOwner{"bag","lot:2",owner,false},AdjustLots{"bag",{{"lot:2",-1}},false},MoveIntegrationLot{"bag","other","lot:2",5,owner,owner}};
    InventoryView clone;clone.id="other";clone.lots=f.inventories.find("bag")->lots;ops.push_back(CreateInventory{clone});
    auto result=f.inspect(std::move(ops));check(result.ok,"all original resource variants inspected");
    if(result.ok){check(result.value.size()==12,"wallet-only four are not cargo touches");for(std::size_t n=0;n<result.value.size();++n){const auto& hit=result.value[n];check(hit.operation_index==n+4,"actual recipe order retained");check(hit.selections.size()==1&&hit.selections.front().inventory_id=="bag"&&hit.selections.front().lot_ids==std::vector<StableId>{"lot:2"},"only original selected lot identified");}}
    auto unrelated=f.inspect({AdjustLots{"bag",{{"lot:1",-1}},false},ConsumeLots{"elsewhere","grain",1,{owner},{}},ConsumeLots{"bag","other",1,{owner},{}}});check(unrelated.ok&&unrelated.value.empty(),"unrelated original lot item and container not selected");
}
// 最终净量/版本相同不能吞掉原recipe中途变动；Owner/预约变化按保守触点保留。
void transient_and_context_changes(){
    Fixture f;auto netzero=f.inspect({AdjustLots{"bag",{{"lot:2",-1}},false},AdjustLots{"bag",{{"lot:2",1}},false}});check(netzero.ok&&netzero.value.size()==2,"no revision net-zero mutation keeps both operations");
    Owner next{Owner::Kind::actor,"next",{}};auto changed=f.inspect({SetLotOwner{"bag","lot:2",next,false},ConsumeLots{"bag","grain",1,{next},{}}});check(changed.ok&&changed.value.size()==2,"later selection cannot use stale original owner to hide touch");
    auto reserve=f.inspect({ReserveLots{"bag","grain",1,{next},{"action:9"}},ReleaseActionResources{"action:9"}});check(reserve.ok&&reserve.value.size()==2,"indirect release remains visible after preceding reserve");
    check(f.inventories.find("bag")->revision==0&&f.inventories.find("bag")->lots.back().quantity==5,"inspection never applies recipe");
    auto empty=inspect_private_shop_freight_resource_scope(f.inventories,{},{{ReleaseActionResources{"action:9"}},{}});check(empty.ok&&empty.value.empty(),"unselected old world has no freight observations");
}
// 原选择只能指恰一实际原批，缺失不能退化成今天同品同量的其他批。
void invalid_sources(){
    Fixture f;f.selected.front().lot_ids={"lot:missing"};auto missing=f.inspect({});check(!missing.ok&&missing.error.original.code=="PrivateShopFreightCargoLotUnavailable","missing original lot refused");
    f.selected.front().lot_ids={"lot:2","lot:2"};check(!f.inspect({}).ok,"duplicate selected lot refused");f.selected.front().lot_ids={"lot:2"};
    auto duplicate=*f.inventories.find("bag");duplicate.id="other";f.inventories.insert(duplicate.id,duplicate);auto alias=f.inspect({});check(!alias.ok&&alias.error.original.code=="PrivateShopFreightCargoLotAmbiguous","same old lot in second inventory refused");
    f.inventories.erase("other");f.inventories.find("bag")->id="wrong";check(!f.inspect({}).ok,"inventory key and payload agree");
}
}
// 该组件只验证观察清单，永久实付/货批/归偿来源未闭前不能启用远地成本入库。
int main(){primitive_coverage();transient_and_context_changes();invalid_sources();std::cout<<"PRIVATE_SHOP_FREIGHT_SCOPE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
