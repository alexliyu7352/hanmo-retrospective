// 以Common原16资源语义的实际临时草稿核批次映射；不替永久库存史或报销资格。
#include "resident/domains/commerce/freight/private_shop_freight_recipe_projection.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 夹具失败即止，保留第一次真实资源拒绝，不继续手造输出。
void check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
void require(bool value,const char* label){check(value,label);if(!value)throw std::runtime_error(label);}
struct Fixture {
    std::shared_ptr<CommonCatalog> catalog=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},catalog};Owner owner{Owner::Kind::actor,"carrier",{}};StableId original,action;
    // 初始容器为空，货批和预约Action均由原primitive/分配器真实建立。
    Fixture(){
        catalog->items.insert("grain",{"grain",1,1,false,false});
        for(const auto& id:{StableId{"carrier"},StableId{"other"}}){ActorRecord a;a.id=id;a.alive=a.available=true;common.state().actors.insert(id,a);}
        for(const auto& id:{StableId{"bag"},StableId{"destination"}}){InventoryView inv;inv.id=id;inv.capacity=100;inv.owner=owner;require(common.apply_resources({{CreateInventory{inv}}, {}}).ok,"actual empty inventory creation");}
        require(common.apply_resources({{AddLot{"bag","grain",8,owner,{}},AddLot{"bag","grain",3,owner,{}}}, {}}).ok,"actual original separate lots");original=common.inventory("bag")->lots.front().id;
        auto id=common.next_id("action");require(id.ok,"actual reservation action allocation");action=id.value;ActionMutation a;a.kind=ActionMutation::Kind::insert;a.action={action,"wait","carrier",{"carrier"},0,1};require(common.apply_action(a).ok,"actual reservation action");require(common.begin_resource_execution_capture().ok,"actual Common execution capture");
    }
    std::vector<PrivateShopFreightCargoSelection> selection()const{return {{"bag",{original}}};}
};
// 一份真实recipe内拆批、转运、消费、改Owner和预约、净零调整都不能丢原身份。
void actual_multi_operation_chain(){
    Fixture f;const auto next=f.common.state().next_instance_id;
    // 此处只预期测试的原全局分配结果；生产投影实际读取Common返回的lot flow，不预测ID。
    const StableId split="lot:"+std::to_string(next),moved="lot:"+std::to_string(next+1);
    ResourceRecipe recipe;recipe.operations={ReserveLots{"bag","grain",3,{f.owner},f.action},TransferLots{"bag","destination","grain",3,{f.owner},f.action,std::nullopt},ConsumeLots{"destination","grain",1,{f.owner},{}},SetLotOwner{"destination",moved,{Owner::Kind::actor,"other",{}},false},ReserveLots{"bag","grain",5,{f.owner},f.action},ReleaseActionResources{f.action},AdjustLots{"destination",{{moved,-1},{moved,1}},false},SetInventoryCapacity{"destination",80},AddLot{"destination","grain",10,f.owner,{}}};
    require(f.common.apply_resources(recipe).ok,"actual complete split transfer consume recipe");require(f.common.resource_execution_drafts().size()==1,"one actual complete recipe frame");const auto& actual=f.common.resource_execution_drafts().front();auto read=project_private_shop_freight_recipe(actual,f.selection());require(read.ok,"exact original lot chain projected");
    check(read.value.operations.size()==9,"all original touched operations retained including net-zero and capacity");check(read.value.remaining_lots.size()==2&&read.value.remaining_lots[0].lot_ids==std::vector<StableId>{f.original}&&read.value.remaining_lots[1].lot_ids==std::vector<StableId>{moved},"only original residual and actual transferred descendant remain");
    check(read.value.operations[0].original_flows.front().destination_lot_id==split&&read.value.operations[1].original_flows.front().source_lot_id==split&&read.value.operations[1].original_flows.front().destination_lot_id==moved,"actual Common split-to-transfer IDs are preserved");
    check(read.value.operations[3].after_lots.front().lot.owner.id=="other"&&read.value.operations[3].after_lots.front().revision==read.value.operations[3].before_lots.front().revision,"no-bump Owner change remains explicit");
    check(read.value.operations[5].after_lots.front().lot.reserved_by.empty()&&read.value.operations[6].original_flows.size()==2,"original release and both net-zero quantity legs retained");
    check(read.value.operations[7].before_lots.front().capacity==100&&read.value.operations[7].after_lots.front().capacity==80,"real container capacity change visible");
    auto broken=actual;broken.operations[0].lot_flows.clear();check(!project_private_shop_freight_recipe(broken,f.selection()).ok,"missing original split mapping refused");
    broken=actual;broken.operations[1].lot_flows.front().source_lot_id=f.original;check(!project_private_shop_freight_recipe(broken,f.selection()).ok,"wrong actual transfer source refused");
    broken=actual;broken.operations[6].lot_flows.pop_back();check(!project_private_shop_freight_recipe(broken,f.selection()).ok,"net-zero adjustment cannot lose one real leg");
    broken=actual;++broken.operations[1].inventories.front().before->capacity;check(!project_private_shop_freight_recipe(broken,f.selection()).ok,"adjacent original container boundary mismatch refused");
    broken=actual;broken.operations[0].lot_flows.front().kind=ResourceLotFlowDraft::Kind::transfer;check(!project_private_shop_freight_recipe(broken,f.selection()).ok,"split cannot change into another primitive kind");
    broken=actual;auto& impossible=std::get<AdjustLots>(broken.recipe.operations[6]);impossible.lots={{moved,-3},{moved,3}};broken.operations[6].lot_flows[0].quantity=-3;broken.operations[6].lot_flows[1].quantity=3;check(!project_private_shop_freight_recipe(broken,f.selection()).ok,"negative intermediate quantity cannot hide behind equal final total");
}
// 同品同量补货不会让已经耗尽的旧批复活，原新lot不是该携运来源。
void consumed_then_replenished(){
    Fixture f;ResourceRecipe recipe;recipe.operations={ConsumeLots{"bag","grain",8,{f.owner},{}},AddLot{"bag","grain",8,f.owner,{}}};require(f.common.apply_resources(recipe).ok,"actual consume and equal replenishment");auto read=project_private_shop_freight_recipe(f.common.resource_execution_drafts().front(),f.selection());require(read.ok,"consumed source still projects");check(read.value.remaining_lots.empty()&&read.value.operations.size()==1,"same-item replacement not inherited by exhausted original lot");
}
// 钱包原语与无关库存不会被说成本批移动；错误起始位置和缺操作帧仍硬拒。
void unrelated_and_bad_selection(){
    Fixture f;require(f.common.apply_resources({{SetInventoryCapacity{"destination",90}}, {}}).ok,"actual unrelated inventory recipe");auto frame=f.common.resource_execution_drafts().front();auto read=project_private_shop_freight_recipe(frame,f.selection());check(read.ok&&read.value.operations.empty()&&read.value.remaining_lots.front().lot_ids==std::vector<StableId>{f.original},"unrelated mutation does not invent selected lot observation");
    check(!project_private_shop_freight_recipe(frame,{{"bag",{f.original,f.original}}}).ok,"duplicate original selection rejected");frame.operations.clear();check(!project_private_shop_freight_recipe(frame,f.selection()).ok,"missing operation frame rejected");
}
// 原整编只限公产，此测试仅观察它的真实同ID搬移，绝不将公产转运授权给私人店铺。
void original_integration_mapping(){
    auto catalog=std::make_shared<CommonCatalog>();catalog->organization_ids={"first","second"};catalog->items.insert("grain",{"grain",1,1,false,false});CommonStore common{CommonState{},catalog};Owner first{Owner::Kind::organization,"first",{}},second{Owner::Kind::organization,"second",{}};
    InventoryView from;from.id="source";from.owner=first;from.capacity=20;InventoryView to;to.id="target";to.owner=second;to.capacity=20;require(common.apply_resources({{CreateInventory{from},CreateInventory{to},AddLot{"source","grain",8,first,{}}}, {}}).ok,"original public integration assets");const auto id=common.inventory("source")->lots.front().id;const auto next=common.state().next_instance_id;
    require(common.begin_resource_execution_capture().ok,"integration real capture");require(common.apply_resources({{MoveIntegrationLot{"source","target",id,8,first,second}}, {}}).ok,"actual original integration primitive");auto read=project_private_shop_freight_recipe(common.resource_execution_drafts().front(),{{"source",{id}}});require(read.ok,"original integration mapped");check(read.value.remaining_lots.size()==1&&read.value.remaining_lots.front().inventory_id=="target"&&read.value.remaining_lots.front().lot_ids==std::vector<StableId>{id}&&common.state().next_instance_id==next,"integration preserves actual ID without allocation");check(read.value.operations.front().after_lots.front().lot.owner.id=="second","actual public Owner change remains explicit without private authority");
}
// 原recipe中途已暂存拆批而末尾拒绝时没有成功草稿，原lot/ID/预约都不发布。
void failed_original_recipe(){
    Fixture f;const auto before=*f.common.inventory("bag");const auto next=f.common.state().next_instance_id;const auto version=f.common.read_version({ResourceKey::Kind::inventory,"bag"});
    auto failed=f.common.apply_resources({{ReserveLots{"bag","grain",3,{f.owner},f.action},ConsumeLots{"bag","grain",999,{f.owner},{}}}, {}});check(!failed.ok&&f.common.resource_execution_drafts().empty(),"failed staged recipe yields no projection input");const auto* after=f.common.inventory("bag");check(after->revision==before.revision&&after->lots.size()==before.lots.size()&&after->lots.front().id==before.lots.front().id&&after->lots.front().quantity==8&&after->lots.front().reserved_by.empty()&&f.common.state().next_instance_id==next&&f.common.read_version({ResourceKey::Kind::inventory,"bag"}).version==version.version,"failed split recipe preserves actual lot reservation IDs and read version");
    require(f.common.apply_resources({{ReserveLots{"bag","grain",8,{f.owner},f.action},ReleaseReservationResources{ReservationRef{f.action}}}, {}}).ok,"actual typed reservation release");auto read=project_private_shop_freight_recipe(f.common.resource_execution_drafts().front(),f.selection());check(read.ok&&read.value.operations.size()==2&&read.value.operations.back().after_lots.front().lot.reserved_by.empty(),"typed release is explicit after whole-lot reservation");
}
}
int main(){try{actual_multi_operation_chain();consumed_then_replenished();unrelated_and_bad_selection();original_integration_mapping();failed_original_recipe();}catch(const std::exception& e){check(false,e.what());}std::cout<<"FREIGHT_RECIPE_PROJECTION_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
