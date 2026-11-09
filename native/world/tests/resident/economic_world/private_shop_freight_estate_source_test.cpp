// 真正原Estate存取货writer与Common批次；初态小庄园为静态夹具，不冒自然经营/永久运输史。
#include "resident/domains/commerce/freight/private_shop_freight_estate_source.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
template<class T>T need(Outcome<T> value,const char* label){if(!value.ok)throw std::runtime_error(std::string(label)+": "+value.error.original.code);check(true,label);return std::move(value.value);}
// 回滚比较原整容器，不以相同总量掩盖Owner/预约/旧批顺序或兼容字段漂移。
bool same_inventory(const InventoryView&a,const InventoryView&b){
 const auto same_owner=[](const Owner&x,const Owner&y){return owner_equal(x,y)&&x.extra==y.extra&&x.field_order==y.field_order;};
 if(a.id!=b.id||!same_owner(a.owner,b.owner)||a.capacity!=b.capacity||a.revision!=b.revision||a.field_order!=b.field_order||a.lots.size()!=b.lots.size())return false;
 for(std::size_t i=0;i<a.lots.size();++i){const auto&x=a.lots[i];const auto&y=b.lots[i];if(x.id!=y.id||x.item_id!=y.item_id||x.quantity!=y.quantity||!same_owner(x.owner,y.owner)||x.reserved_by!=y.reserved_by||x.extra!=y.extra||x.field_order!=y.field_order)return false;}return true;
}
struct Activities final:ActivityRead {bool in_open_battle(const StableId&)const override{return false;}bool in_interaction(const StableId&,const StableId&)const override{return false;}};
struct Fixture {
 std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},shared};EstateState state;EstateCatalog catalog;Activities activity;
 StableId first,second,old_dest;
 Fixture(){
  shared->places.insert("farm",{"farm","town",{}});shared->items.insert("grain",{"grain",1,1});ActorRecord actor;actor.id="owner";actor.wallet_id="owner.cash";actor.inventory_id="owner.bag";actor.alive=actor.available=true;actor.hp=100;actor.location=SettlementLocation{"town","farm"};common.state().actors.insert(actor.id,actor);
  const Owner owner{Owner::Kind::actor,"owner",{}};InventoryView bag{"owner.bag",owner,100,0,{}},stock{"estate.stock",owner,100,0,{}};check(common.apply_resources({{CreateInventory{bag},CreateInventory{stock}}, {}}).ok,"actual original private containers");
  EstateDefinition d;d.id="estate.original";d.owner_actor_id="owner";d.inventory_id=stock.id;d.place_id="farm";catalog.estates.insert(d.id,d);EstateRecord e;e.id=d.id;e.owner_actor_id="owner";state.estates.insert(e.id,e);
  check(common.apply_resources({{AddLot{bag.id,"grain",2,owner,{}},AddLot{bag.id,"grain",5,owner,{}},AddLot{stock.id,"grain",4,owner,{}}}, {}}).ok,"real original source fragments and old destination grain");first=common.inventory(bag.id)->lots[0].id;second=common.inventory(bag.id)->lots[1].id;old_dest=common.inventory(stock.id)->lots.front().id;
  check(common.begin_resource_execution_capture().ok,"begin original successful recipe capture");
 }
 PrivateShopFreightEstateContext context(){return {common,state,catalog};}
 EstateQuote quote(bool deposit=true,Integer quantity=4){return need(preview_estate(common,state,catalog,*shared,activity,"owner",EstateTransfer{"estate.original","grain",quantity,{},deposit,false}),"actual original private estate quote");}
 EstateReceipt apply(const EstateQuote& q){return need(apply_estate(common,state,catalog,*shared,activity,"owner",q),"actual original estate TransferLots and fact writer");}
};
void deposit_and_withdraw(){
 Fixture f;auto q=f.quote();auto before=need(capture_private_shop_freight_estate_boundary(f.context(),"owner",q),"private original boundary");auto receipt=f.apply(q);const auto count=f.common.state().next_instance_id;auto source=need(read_private_shop_freight_estate_transfer(f.context(),before,receipt.fact_id),"original private deposit source");
 const auto& flows=source.actual_recipe.operations.front().lot_flows;check(source.original_transfer.transfer&&flows.size()==2&&flows[0].source_lot_id==f.first&&flows[0].quantity==2&&flows[1].source_lot_id==f.second&&flows[1].quantity==2,"original FIFO split retains both actual source IDs");check(f.common.inventory("owner.bag")->lots.front().id==f.second&&f.common.inventory("owner.bag")->lots.front().quantity==3&&f.common.inventory("estate.stock")->lots.front().id==f.old_dest,"real partial original lot and old stored grain retained");check(f.common.state().next_instance_id==count,"pure source allocates no ID");
 auto withdraw=f.quote(false,3);auto reverse=need(capture_private_shop_freight_estate_boundary(f.context(),"owner",withdraw),"actual withdrawal boundary");auto returned=f.apply(withdraw);auto out=need(read_private_shop_freight_estate_transfer(f.context(),reverse,returned.fact_id),"actual original withdrawal source");check(!std::get<EstateTransfer>(out.original_transfer.command).deposit&&out.actual_recipe.operations.front().lot_flows.front().source_lot_id==f.old_dest,"withdrawal uses actual oldest estate batch, not newly deposited cargo by same item");
 check(!read_private_shop_freight_estate_transfer(f.context(),before,receipt.fact_id).ok,"later real withdrawal cannot be hidden inside earlier deposit window");
}
void mismatches(){
 Fixture f;auto q=f.quote();auto before=need(capture_private_shop_freight_estate_boundary(f.context(),"owner",q),"bad source original boundary");auto r=f.apply(q);auto& d=const_cast<ResourceExecutionDraft&>(f.common.resource_execution_drafts().back());const auto saved=d;
 d.operations.front().lot_flows.front().destination_lot_id=f.old_dest;check(!read_private_shop_freight_estate_transfer(f.context(),before,r.fact_id).ok,"old same-item destination is not a fresh transferred lot");d=saved;
 d.operations.front().lot_flows.pop_back();check(!read_private_shop_freight_estate_transfer(f.context(),before,r.fact_id).ok,"missing real source fragment rejected");d=saved;
 d.operations.front().inventories.front().after.capacity++;check(!read_private_shop_freight_estate_transfer(f.context(),before,r.fact_id).ok,"wrong actual container frame rejected");d=saved;
 auto* fact=std::get_if<EstateManagementFact>(f.state.facts.find(r.fact_id));const auto old=*fact;fact->transfer->quantity++;check(!read_private_shop_freight_estate_transfer(f.context(),before,r.fact_id).ok,"original terms and actual resource quantity must agree");*fact=old;
 std::get<EstateTransfer>(fact->command).deposit=false;check(!read_private_shop_freight_estate_transfer(f.context(),before,r.fact_id).ok,"opposite original direction rejected");*fact=old;
 auto bad=before;bad.original_common=nullptr;check(!read_private_shop_freight_estate_transfer(f.context(),bad,r.fact_id).ok,"detached candidate cannot act as history");bad=before;++bad.identity.tick;check(!read_private_shop_freight_estate_transfer(f.context(),bad,r.fact_id).ok,"another clock cannot backfill this transfer");
 f.state.estates.find("estate.original")->owner_actor_id="other";check(!read_private_shop_freight_estate_transfer(f.context(),before,r.fact_id).ok,"ownership change cannot preserve private freight eligibility");f.state.estates.find("estate.original")->owner_actor_id="owner";
 check(read_private_shop_freight_estate_transfer(f.context(),before,r.fact_id).ok,"exact restored original transfer remains readable");
}
void private_scope_and_late_failure(){
 Fixture f;auto q=f.quote();auto bad=q;std::get<EstateTransfer>(bad.command).source_id="public.source";bad.transfer->source_id="public.source";check(!capture_private_shop_freight_estate_boundary(f.context(),"owner",bad).ok,"original public authority not reclassified as private owner deposit");
 f.state.estates.find("estate.original")->owner_actor_id="other";f.state.estates.find("estate.original")->manager_actor_id="owner";check(!capture_private_shop_freight_estate_boundary(f.context(),"owner",q).ok,"manager permission does not make another person's estate personal cargo");
 Fixture late;late.common.state().next_instance_id=std::numeric_limits<Integer>::max()-2;auto quoted=late.quote();auto boundary=need(capture_private_shop_freight_estate_boundary(late.context(),"owner",quoted),"late original boundary");const auto bag=*late.common.inventory("owner.bag"),stock=*late.common.inventory("estate.stock");const auto original_next=late.common.state().next_instance_id;const auto bag_version=late.common.read_version({ResourceKey::Kind::inventory,bag.id}),stock_version=late.common.read_version({ResourceKey::Kind::inventory,stock.id});auto point=need(late.common.begin_savepoint(),"original outer resource savepoint");auto result=apply_estate(late.common,late.state,late.catalog,*late.shared,late.activity,"owner",quoted);
 check(!result.ok&&late.state.facts.empty()&&late.common.resource_execution_drafts().size()==1,"original two lot allocations succeed but final estate fact ID fails");check(!read_private_shop_freight_estate_transfer(late.context(),boundary,"estate:"+std::to_string(std::numeric_limits<Integer>::max())).ok,"paid resource shape without real source head rejected");
 check(late.common.state().next_instance_id==std::numeric_limits<Integer>::max()&&late.common.inventory("owner.bag")->lots.size()==1&&late.common.inventory("owner.bag")->lots.front().id==late.second&&late.common.inventory("owner.bag")->lots.front().quantity==3&&late.common.inventory("estate.stock")->lots.size()==3,"late failure is really after publishing both transferred batches");
 check(late.common.rollback_savepoint(point).ok,"original candidate rollback");check(late.common.state().next_instance_id==original_next&&late.common.resource_execution_drafts().empty()&&same_inventory(bag,*late.common.inventory(bag.id))&&same_inventory(stock,*late.common.inventory(stock.id)),"actual rollback restores complete original containers and temporary source prefix");check(late.common.read_version(bag_version.resource).version==bag_version.version&&late.common.read_version(stock_version.resource).version==stock_version.version,"original resource read versions also roll back");
}
// 同址重开或同址赋值不能拿旧临时前界读取后来的庄园Transfer；新前界仍认原writer。
void capture_lifecycle(){
 for(bool assignment:{false,true}){Fixture f;auto q=f.quote();auto old=need(capture_private_shop_freight_estate_boundary(f.context(),"owner",q),"old estate capture boundary");
  if(assignment){CommonStore copy=f.common;check(!copy.resource_execution_capture_identity(),"copy has no active estate capture");f.common=copy;check(!f.common.resource_execution_capture_identity(),"assignment clears old estate capture");}
  else check(f.common.finish_resource_execution_capture().ok,"finish old estate capture");
  check(!capture_private_shop_freight_estate_boundary(f.context(),"owner",q).ok,"inactive estate boundary refused");check(f.common.begin_resource_execution_capture().ok,"new capture at original Common address");auto fresh=need(capture_private_shop_freight_estate_boundary(f.context(),"owner",q),"new estate boundary");auto done=f.apply(q);
  check(!read_private_shop_freight_estate_transfer(f.context(),old,done.fact_id).ok,"old estate identity rejected despite matching values and address");check(read_private_shop_freight_estate_transfer(f.context(),fresh,done.fact_id).ok,"new estate identity matches actual transfer");}
}

}
int main(){try{deposit_and_withdraw();mismatches();private_scope_and_late_failure();capture_lifecycle();std::cout<<checks<<"/0 original estate freight source checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
