// 同一原Common资源写者的临时捕获门；不构造World或把执行草稿当永久来源。
#include "resident/storage/common_codec.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <iostream>
#include <limits>
#include <set>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
void check(bool value,const char* message){++checks;if(!value){++failures;std::cerr<<message<<'\n';}}
Owner person(const StableId& id){return {Owner::Kind::actor,id,{},{}};}
Owner organization(const StableId& id){return {Owner::Kind::organization,id,{},{}};}
CommonState initial(){
 CommonState s;s.identity.tick=3;s.identity.revision=9;s.next_instance_id=100;
 s.wallets.set("w1",{"w1",person("p"),100,{},{}});s.wallets.set("w2",{"w2",person("q"),0,{},{}});
 s.inventories.set("i1",{"i1",person("p"),100,0,{{"seed","goods",{},5,person("p"),{},{}}},{}});
 s.inventories.set("i2",{"i2",person("q"),100,0,{},{}});
 s.inventories.set("mi1",{"mi1",organization("o1"),100,0,{{"public-lot","goods",{},2,organization("o1"),{},{}}},{}});
 s.inventories.set("mi2",{"mi2",organization("o2"),100,0,{},{}});
 return s;
}
std::shared_ptr<CommonCatalog> catalog(){auto c=std::make_shared<CommonCatalog>();c->items.set("goods",{"goods",1,1,false,false});c->organization_ids={"o1","o2"};return c;}
bool same(const CommonStore& a,const CommonStore& b){return compare_boundary(encode_common_state(a.state()),encode_common_state(b.state())).exact();}
struct Fixture {
 std::shared_ptr<CommonCatalog> definitions=catalog();CommonStore plain{initial(),definitions},captured{initial(),definitions};std::set<std::size_t> kinds;
 Fixture(){check(captured.begin_resource_execution_capture().ok,"begin actual temporary capture");}
 bool apply(ResourceRecipe recipe){for(const auto& op:recipe.operations)kinds.insert(op.index());const auto count=captured.resource_execution_drafts().size();auto a=plain.apply_resources(recipe),b=captured.apply_resources(recipe);check(a.ok==b.ok&&a.error.original.code==b.error.original.code,"capture preserves original status and first failure");check(same(plain,captured),"capture preserves exact Common bytes");check(captured.resource_execution_drafts().size()==count+(b.ok?1:0),"only whole successful recipe appends");return b.ok;}
 const ResourceExecutionDraft& last()const{return captured.resource_execution_drafts().back();}
};
// 热捕获与持久有限效果只借同一批真实帧；核新借用入口不复制或重放原语。
template<class Operation>void selected_evidence_case(const Operation& operation){
 Fixture selected;check(selected.apply({{operation},{}}),"selected original writer captures real frames");
 const auto& actual=selected.last().operations.front();
 const SelectedResourceExecutionEvidenceView view{actual.wallets,actual.inventories,actual.lot_flows,actual.next_instance_before,actual.next_instance_after};
 check(view.inventories.data()==actual.inventories.data()&&view.lot_flows.data()==actual.lot_flows.data(),"evidence borrows original frames without scratch draft");
 check(validate_selected_resource_execution(ResourceOperation{operation},actual).ok&&validate_selected_resource_evidence(operation,view).ok,"hot and typed evidence accept same real operation");
 auto rejected=[&](const ResourceOperationExecutionDraft& changed){
  auto hot=validate_selected_resource_execution(ResourceOperation{operation},changed);
  auto cold=validate_selected_resource_evidence(operation,{changed.wallets,changed.inventories,changed.lot_flows,changed.next_instance_before,changed.next_instance_after});
  check(!hot.ok&&!cold.ok&&hot.error.original.code==cold.error.original.code,"hot and borrowed evidence preserve same first rejection");
 };
 auto changed=actual;++changed.lot_flows.front().quantity;rejected(changed);
 changed=actual;++changed.next_instance_after;rejected(changed);
 changed=actual;changed.inventories.front().after.capacity+=1;rejected(changed);
 changed=actual;changed.wallets.push_back({"foreign",{}, {}});rejected(changed);
 const auto unselected=validate_selected_resource_evidence(ResourceOperation{WalletDelta{"w1",1,true}},view);
 check(!unselected.ok&&unselected.error.original.code=="SelectedResourceOperationRequired","borrowed view does not authorize another operation kind");
 check(same(selected.plain,selected.captured),"evidence validation never mutates Common");
}
// 原市场拒绝发生在共同发布前，不能把第二份外域observer当草稿所有者。
struct RejectMarket final:ProtectedMarketResourceObserver {
 bool observed=false,finish_refused=false;
 Status inspect_market_recipe(const CommonStore&,const ResourceRecipe&)const override{return common_success();}
 Status capture_market_recipe(const CommonStore& c,const ResourceRecipe&,const OrderedTable<InventoryView>& stage,Integer)override{observed=stage.find("i1")!=nullptr;finish_refused=!const_cast<CommonStore&>(c).finish_resource_execution_capture().ok;return common_failure("TestMarketRejected");}
};
}
int main(){
 Fixture f;
 check(!f.captured.begin_resource_execution_capture().ok,"nested capture rejected");
 check(f.apply({{WalletDelta{"w1",-7,true},WalletDelta{"w1",7,true}},{}}),"original net zero wallet recipe");
 check(f.last().operations.size()==2&&f.last().operations[0].wallets[0].after.balance==93&&f.last().operations[1].wallets[0].before->balance==93&&f.last().operations[1].wallets[0].after.balance==100,"net zero does not hide original debit order");
 check(f.last().identity.tick==3&&f.last().identity.revision==9,"actual Common identity retained without future tick");
 check(f.apply({{ReserveLots{"i1","goods",2,{person("p")},ReservationRef{"action:1"}}},{}}),"actual split reservation");
 const auto split=f.last().operations[0].lot_flows.at(0);
 check(split.kind==ResourceLotFlowDraft::Kind::reserve_split&&split.source_lot_id=="seed"&&split.destination_lot_id=="lot:100"&&split.quantity==2,"split maps original and generated lot");
 check(f.last().next_instance_before==100&&f.last().next_instance_after==101,"actual generated ID interval");
 check(f.apply({{SetReservation{"w1",ReservationRef{"action:1"},8,false},ReleaseActionResources{"action:1"}},{}}),"indirect action release original writer");
 check(f.last().operations[1].wallets.size()==1&&f.last().operations[1].inventories.size()==1&&f.last().operations[1].wallets[0].before->reserved.size()==1&&f.last().operations[1].wallets[0].after.reserved.empty(),"release records both actual affected tables");
 check(f.apply({{TransferLots{"i1","i2","goods",4,{person("p")},{},person("q")}},{}}),"real FIFO transfer");
 const auto flows=f.last().operations[0].lot_flows;
 check(flows.size()==2&&flows[0].source_lot_id=="seed"&&flows[0].destination_lot_id=="lot:101"&&flows[0].quantity==3&&flows[1].source_lot_id=="lot:100"&&flows[1].destination_lot_id=="lot:102"&&flows[1].quantity==1,"each actual transfer source maps to its original produced lot");
 check(f.apply({{ReserveLots{"i1","goods",1,{person("p")},ReservationRef{"action:2"}}},{}}),"whole reservation");
 check(f.last().operations[0].lot_flows[0].kind==ResourceLotFlowDraft::Kind::reserve_whole&&f.last().operations[0].lot_flows[0].source_lot_id==f.last().operations[0].lot_flows[0].destination_lot_id&&f.last().next_instance_before==f.last().next_instance_after,"whole reservation keeps original ID");
 const ReservationRef contract{ReservationRef::Kind::contract,"contract:3"};
 check(f.apply({{SetReservation{"w1",contract,9,false},ReleaseReservationResources{contract}},{}}),"typed contract release");
 check(f.apply({{ConsumeLots{"i2","goods",2,{person("q")},{}}},{}}),"real consume");
 check(f.last().operations[0].lot_flows[0].source_lot_id=="lot:101"&&f.last().operations[0].lot_flows[0].quantity==2&&f.last().operations[0].lot_flows[0].destination_lot_id.empty(),"consume keeps actual source without fabricated destination");
 check(f.apply({{AdjustLots{"i1",{{"lot:100",2}},false}},{}}),"revision preserving adjustment");
 check(f.last().operations[0].inventories[0].before->revision==f.last().operations[0].inventories[0].after.revision&&f.last().operations[0].lot_flows[0].quantity==2,"revision alone cannot hide actual adjustment");
 check(f.apply({{SetInventoryCapacity{"i1",200},SetInventoryOwner{"i1",person("q")},SetLotOwner{"i1","lot:100",person("q"),false},SetWalletOwner{"w1",person("q")}},{}}),"actual ownership and capacity writers");
 check(f.last().operations[1].inventories[0].before->owner.id=="p"&&f.last().operations[1].inventories[0].after.owner.id=="q","owner values retained without a revision heuristic");
 check(f.apply({{CreateWallet{{"new-wallet",person("p"),0,{},{}}},CreateInventory{{"new-box",person("p"),10,0,{},{}}},AddLot{"new-box","goods",3,person("p"),{}}},{}}),"create and add in same real recipe");
 check(!f.last().operations[0].wallets[0].before&&!f.last().operations[1].inventories[0].before&&f.last().operations[2].inventories[0].before->lots.empty(),"creation empty before differs from later empty container");
 check(f.apply({{MoveIntegrationLot{"mi1","mi2","public-lot",2,organization("o1"),organization("o2")}},{}}),"original organization integration move");
 check(f.last().operations[0].lot_flows[0].kind==ResourceLotFlowDraft::Kind::integration_move&&f.last().operations[0].lot_flows[0].source_lot_id=="public-lot"&&f.last().operations[0].lot_flows[0].destination_lot_id=="public-lot","integration preserves actual lot identity");
 check(f.kinds.size()==16,"all original resource operation alternatives exercised");
 check(f.apply({{AddLot{"new-box","goods",0,person("p"),{}}},{}}),"zero quantity original operation");
 check(f.last().operations.size()==1&&f.last().operations[0].lot_flows.empty()&&f.last().next_instance_before==f.last().next_instance_after,"zero operation retained without fabricated lot");
 const auto before=encode_common_state(f.captured.state());const auto count=f.captured.resource_execution_drafts().size();
 check(!f.apply({{WalletDelta{"w1",1,true},ConsumeLots{"new-box","goods",999,{person("p")},{}}},{}}),"late original shortage refuses whole staged recipe");
 check(compare_boundary(before,encode_common_state(f.captured.state())).exact()&&f.captured.resource_execution_drafts().size()==count,"failed recipe leaves no asset or execution row");
 auto a=f.plain.begin_savepoint(),b=f.captured.begin_savepoint();check(a.ok&&b.ok,"original savepoints open while capturing");
 check(!f.captured.finish_resource_execution_capture().ok,"cannot consume draft inside rollback boundary");
 check(f.apply({{WalletDelta{"w1",5,true}},{}}),"real write after savepoint");
 check(f.plain.rollback_savepoint(a.value).ok&&f.captured.rollback_savepoint(b.value).ok,"original savepoint rollback succeeds");
 check(same(f.plain,f.captured)&&f.captured.resource_execution_drafts().size()==count,"rollback restores exact capture prefix and bytes");
 a=f.plain.begin_savepoint();b=f.captured.begin_savepoint();check(f.apply({{WalletDelta{"w1",1,true}},{}}),"write before commit");check(f.plain.commit_savepoint(a.value).ok&&f.captured.commit_savepoint(b.value).ok&&f.captured.resource_execution_drafts().size()==count+1,"savepoint commit keeps actual draft");
 CommonStore clone(f.captured);check(clone.resource_execution_drafts().empty()&&!clone.finish_resource_execution_capture().ok&&same(clone,f.captured),"copy keeps state but does not inherit capture authority");
 auto taken=f.captured.finish_resource_execution_capture();check(taken.ok&&taken.value.size()==count+1&&f.captured.resource_execution_drafts().empty(),"one explicit same-stack draft delivery");
 check(!f.captured.finish_resource_execution_capture().ok,"cannot consume a capture twice");
 Fixture reject;reject.definitions->protected_market_inventory_ids={"i1"};RejectMarket observer;reject.plain.bind_protected_market_resources(&observer);reject.captured.bind_protected_market_resources(&observer);
 const auto rejected_before=encode_common_state(reject.captured.state());check(!reject.apply({{AddLot{"i1","goods",1,person("p"),{}}},{}}),"original market guard rejection retained");check(observer.observed&&observer.finish_refused&&reject.captured.resource_execution_drafts().empty()&&compare_boundary(rejected_before,encode_common_state(reject.captured.state())).exact(),"market rejection cannot publish resources or capture draft");
 CommonStore moved(std::move(f.captured));check(moved.resource_execution_drafts().empty()&&!moved.finish_resource_execution_capture().ok,"move has no inherited capture");
 // active捕获及保存点的真实移动必须保原资源撤销，不能半恢复后报告捕获边界失败。
 for(bool assign:{false,true}){
  Fixture moving;check(moving.apply({{WalletDelta{"w1",-2,true}},{}}),"actual prefix before move savepoint");
  const auto saved_bytes=encode_common_state(moving.captured.state());const auto saved_version=moving.captured.read_version({ResourceKey::Kind::wallet,"w1"});
  auto point=moving.captured.begin_savepoint();check(point.ok,"active capture opens real savepoint");
  check(moving.captured.apply_resources({{WalletDelta{"w1",-3,true},AddLot{"i1","goods",1,person("p"),{}}},{}}).ok,"actual multi-resource write before move");
  CommonStore target=assign?CommonStore(initial(),catalog()):CommonStore(std::move(moving.captured));
  if(assign){check(target.begin_resource_execution_capture().ok,"assignment target has its own temporary capture");target=std::move(moving.captured);}
  check(target.resource_execution_drafts().empty()&&!target.finish_resource_execution_capture().ok&&moving.captured.resource_execution_drafts().empty(),"move clears both temporary captures");
  check(!target.begin_resource_execution_capture().ok,"moved original savepoint still prevents a new capture");
  check(target.rollback_savepoint(point.value).ok,"moved original savepoint rolls back successfully");
  check(compare_boundary(saved_bytes,encode_common_state(target.state())).exact()&&target.read_version({ResourceKey::Kind::wallet,"w1"}).version==saved_version.version,"move rollback restores bytes and versions exactly");
  check(target.begin_resource_execution_capture().ok&&target.finish_resource_execution_capture().ok,"new same-stack capture allowed only after original boundary ends");
 }
 Fixture active_move;check(active_move.apply({{WalletDelta{"w1",-1,true}},{}}),"active no-savepoint move source");
 CommonStore moved_active(std::move(active_move.captured));check(same(moved_active,active_move.plain)&&moved_active.resource_execution_drafts().empty()&&!moved_active.finish_resource_execution_capture().ok,"active move preserves resources and drops temporary proof");
 Fixture copied;auto copied_point=copied.captured.begin_savepoint();check(copied_point.ok,"copy source savepoint");
 CommonStore assigned(initial(),catalog());assigned=copied.captured;check(!assigned.rollback_savepoint(copied_point.value).ok&&assigned.resource_execution_drafts().empty()&&!assigned.finish_resource_execution_capture().ok,"copy assignment inherits neither savepoint nor capture");
 check(copied.captured.rollback_savepoint(copied_point.value).ok,"copy leaves original capture boundary intact");
 selected_evidence_case(ReserveSelectedLots{"i1",{{"i1","seed",2}},{person("p")},ReservationRef{"action:selected"}});
 selected_evidence_case(TransferSelectedLots{"i1","i2",{{"i1","seed",2}},{person("p")},{},person("q")});
 selected_evidence_case(ConsumeSelectedLots{"i1",{{"i1","seed",2}},{person("p")},{}});
 std::cout<<"RESOURCE_EXECUTION_DRAFT="<<checks<<" FAILURES="<<failures<<" SCOPE=COMMON_TEMPORARY_ONLY\n";return failures?1:0;
}
