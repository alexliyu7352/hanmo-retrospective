// 原Common实际写者的同次覆盖门；不把端点相等当零净直接写从未发生。
#include "resident/storage/resource_execution_coverage.hpp"
#include "resident/storage/common_store.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 必要坏边界当场停，不把已执行检查数冒整World验收。
void need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Owner person(const StableId& id){return {Owner::Kind::actor,id,{},{}};}
Owner org(const StableId& id){return {Owner::Kind::organization,id,{},{}};}
// 静态原资产夹具只支撑原资源数学，未创建游戏人物资格。
CommonStore fixture(){auto c=std::make_shared<CommonCatalog>();c->items.set("goods",{"goods",1,1,false,false});c->organization_ids={"o1","o2"};CommonState s;s.identity.tick=3;s.identity.revision=9;s.next_instance_id=100;
 s.wallets.set("w1",{"w1",person("p"),100,{},{}});s.wallets.set("w2",{"w2",person("q"),0,{},{}});
 s.inventories.set("i1",{"i1",person("p"),100,0,{{"seed","goods",{},5,person("p"),{},{}}},{}});s.inventories.set("i2",{"i2",person("q"),100,0,{},{}});
 s.inventories.set("mi1",{"mi1",org("o1"),100,0,{{"public-lot","goods",{},2,org("o1"),{},{}}},{}});s.inventories.set("mi2",{"mi2",org("o2"),100,0,{},{}});return CommonStore(std::move(s),std::move(c));}
// 失败来源不能发布：当前受损候选保持可诊断，真正前世界一直独立未变。
void rejects_direct(const std::function<void(CommonStore&)>& edit){auto original=fixture();CommonStore candidate(original);auto boundary=begin_resource_execution_coverage(candidate);need(boundary.ok,"begin coverage");edit(candidate);auto out=finish_resource_execution_coverage(candidate,boundary.value);need(!out.ok&&out.value.empty(),"unrecorded difference published");need(original.wallet("w1")->balance==100&&original.inventory("i1")->capacity==100&&original.inventory("i1")->lots[0].quantity==5,"failed candidate affected original");}
}
int main(){try{
 auto c=fixture();auto boundary=begin_resource_execution_coverage(c);need(boundary.ok,"begin actual capture");need(!begin_resource_execution_coverage(c).ok,"nested capture allowed");
 std::vector<ResourceRecipe> recipes={
  {{WalletDelta{"w1",-7,true},WalletDelta{"w1",7,true}},{}},
  {{ReserveLots{"i1","goods",2,{person("p")},ReservationRef{"action:1"}},SetReservation{"w1",ReservationRef{"action:1"},8,false},ReleaseActionResources{"action:1"}},{}},
  {{TransferLots{"i1","i2","goods",4,{person("p")},{},person("q")}},{}},
  {{ReserveLots{"i1","goods",1,{person("p")},ReservationRef{"action:2"}},ReleaseReservationResources{ReservationRef{"action:2"}}},{}},
  {{ConsumeLots{"i2","goods",2,{person("q")},{}},AdjustLots{"i1",{{"lot:100",2}},false}},{}},
  {{SetInventoryCapacity{"i1",200},SetInventoryOwner{"i1",person("q")},SetLotOwner{"i1","lot:100",person("q"),false},SetWalletOwner{"w1",person("q")}},{}},
  {{CreateWallet{{"new-wallet",person("p"),0,{},{}}},CreateInventory{{"new-box",person("p"),10,0,{},{}}},AddLot{"new-box","goods",3,person("p"),{}}},{}},
  {{MoveIntegrationLot{"mi1","mi2","public-lot",2,org("o1"),org("o2")}},{}},
  {{AddLot{"new-box","goods",0,person("p"),{}}},{}}
 };
 for(const auto& recipe:recipes)need(c.apply_resources(recipe).ok,"original recipe failed");
 auto output=finish_resource_execution_coverage(c,boundary.value);need(output.ok&&output.value.size()==recipes.size(),"actual original operations not closed");need(!finish_resource_execution_coverage(c,boundary.value).ok,"reused boundary accepted");
 rejects_direct([](auto& s){s.state().wallets.find("w1")->balance+=1;});
 rejects_direct([](auto& s){s.state().wallets.find("w1")->owner.id="q";});
 rejects_direct([](auto& s){s.state().inventories.find("i1")->capacity+=1;});
 rejects_direct([](auto& s){s.state().inventories.find("i1")->lots[0].quantity+=1;});
 rejects_direct([](auto& s){s.state().inventories.find("i1")->lots[0].owner.id="q";});
 rejects_direct([](auto& s){s.state().inventories.find("i1")->lots[0].reserved_by=ReservationRef{"action:1"};});
 rejects_direct([](auto& s){s.state().wallets.find("w1")->field_order={"balance","owner","reserved"};});
 rejects_direct([](auto& s){auto value=*s.wallet("w1");s.state().wallets.erase("w1");s.state().wallets.insert("w1",value);});
 // 直接变化被后来真实反向原语抵消，记录前值仍暴露该中间缺口。
 rejects_direct([](auto& s){s.state().wallets.find("w1")->balance+=5;need(s.apply_resources({{WalletDelta{"w1",-5,true}}, {}}).ok,"compensating writer failed");});
 auto rollback=fixture();auto rb=begin_resource_execution_coverage(rollback);need(rb.ok,"rollback start");auto point=rollback.begin_savepoint();need(point.ok,"savepoint start");need(rollback.apply_resources({{WalletDelta{"w1",-3,true},AddLot{"i2","goods",2,person("q"),{}}},{}}).ok,"savepoint actual writes");need(!finish_resource_execution_coverage(rollback,rb.value).ok,"open savepoint consumed");need(rollback.rollback_savepoint(point.value).ok,"rollback failed");auto undone=finish_resource_execution_coverage(rollback,rb.value);need(undone.ok&&undone.value.empty()&&rollback.wallet("w1")->balance==100,"rollback prefix did not close");
 auto cross=fixture();auto bound=begin_resource_execution_coverage(cross);need(bound.ok,"cross begin");CommonStore copy(cross);need(!finish_resource_execution_coverage(copy,bound.value).ok,"different Common stole proof");need(finish_resource_execution_coverage(cross,bound.value).ok,"wrong candidate consumed original boundary");
 auto late=fixture();auto lb=begin_resource_execution_coverage(late);need(lb.ok,"late begin");need(!late.apply_resources({{WalletDelta{"w1",-5,true},ConsumeLots{"i1","goods",999,{person("p")},{}}},{}}).ok,"late resource shortage accepted");auto no_write=finish_resource_execution_coverage(late,lb.value);need(no_write.ok&&no_write.value.empty()&&late.wallet("w1")->balance==100,"failed recipe published resources");
 // 空recipe同样不能掩盖实例号倒退；正常独立域分配的向前间隙仍被允许。
 rejects_direct([](auto& s){s.state().next_instance_id-=1;});
 auto gap=fixture();auto gb=begin_resource_execution_coverage(gap);need(gb.ok,"gap begin");need(gap.next_id("source").ok,"domain id failed");need(gap.apply_resources({{WalletDelta{"w1",-1,true}}, {}}).ok,"gap resource failed");need(finish_resource_execution_coverage(gap,gb.value).ok,"real domain id gap rejected");
 // 每次捕获自己的真实时钟也要前进，不能先记未来再恢复到过去后伪作合法末态。
 rejects_direct([](auto& s){s.state().identity.tick=5;need(s.apply_resources({{WalletDelta{"w1",-1,true}}, {}}).ok,"future first failed");s.state().identity.tick=4;need(s.apply_resources({{WalletDelta{"w1",1,true}}, {}}).ok,"past second failed");s.state().identity.tick=5;});
 // 原捕获被旁路结束再开启时，旧边界不能拿第二次零净草稿冒第一次完整执行。
 auto reopened=fixture();auto original_capture=begin_resource_execution_coverage(reopened);need(original_capture.ok,"identity begin");need(reopened.apply_resources({{WalletDelta{"w1",-1,true},WalletDelta{"w1",1,true}}, {}}).ok,"identity actual write");need(reopened.finish_resource_execution_capture().ok,"raw finish failed");need(reopened.begin_resource_execution_capture().ok,"raw restart failed");need(!finish_resource_execution_coverage(reopened,original_capture.value).ok,"old boundary consumed restarted capture");
 // 旧边界拒绝时不偷走真正第二次捕获；copy同地址赋值和move双方均断开旧身份。
 need(reopened.finish_resource_execution_capture().ok,"stale boundary consumed new capture");
 auto replaced=fixture();auto replaced_boundary=begin_resource_execution_coverage(replaced);need(replaced_boundary.ok,"replacement begin");auto fresh=fixture();replaced=fresh;need(replaced.begin_resource_execution_capture().ok,"replacement restart");need(!finish_resource_execution_coverage(replaced,replaced_boundary.value).ok,"copy assignment reused identity");
 auto moved_from=fixture();auto moved_boundary=begin_resource_execution_coverage(moved_from);need(moved_boundary.ok,"move begin");CommonStore moved(std::move(moved_from));need(!moved.resource_execution_capture_identity()&&!moved_from.resource_execution_capture_identity(),"move retained identity");need(!finish_resource_execution_coverage(moved,moved_boundary.value).ok,"move stole boundary");
 auto assigned=fixture();auto assigned_boundary=begin_resource_execution_coverage(assigned);need(assigned_boundary.ok,"assignment begin");auto source=fixture();auto source_boundary=begin_resource_execution_coverage(source);need(source_boundary.ok,"assignment source begin");assigned=std::move(source);need(!assigned.resource_execution_capture_identity()&&!source.resource_execution_capture_identity(),"move assignment kept identity");need(assigned.begin_resource_execution_capture().ok,"assigned restart");need(!finish_resource_execution_coverage(assigned,assigned_boundary.value).ok,"move assignment reused old boundary");
 auto committed=fixture();auto cb=begin_resource_execution_coverage(committed);need(cb.ok,"commit begin");auto copy_boundary=cb.value;auto identity=committed.resource_execution_capture_identity();auto cp=committed.begin_savepoint();need(cp.ok,"commit savepoint");need(committed.apply_resources({{WalletDelta{"w1",-1,true}}, {}}).ok,"commit writer");need(committed.commit_savepoint(cp.value).ok,"commit savepoint failed");need(identity==committed.resource_execution_capture_identity(),"commit changed capture identity");need(finish_resource_execution_coverage(committed,copy_boundary).ok,"same capture commit rejected");need(!finish_resource_execution_coverage(committed,cb.value).ok,"copied boundary reused");
 // 明确能力上限：没经过记录且已完全还原的直接写，不能由端点判断它是否发生。
 auto neutral=fixture();auto nb=begin_resource_execution_coverage(neutral);need(nb.ok,"neutral begin");neutral.state().wallets.find("w1")->balance-=1;neutral.state().wallets.find("w1")->balance+=1;auto net=finish_resource_execution_coverage(neutral,nb.value);need(net.ok&&net.value.empty(),"endpoint coverage silently promises trace completeness");
 std::cout<<"RESOURCE_EXECUTION_COVERAGE "<<checks<<"/0 COMMON_CANDIDATE_ONLY\n";return 0;
}catch(const std::exception& e){std::cerr<<"RESOURCE_EXECUTION_COVERAGE "<<checks<<"/1 "<<e.what()<<'\n';return 1;}}
