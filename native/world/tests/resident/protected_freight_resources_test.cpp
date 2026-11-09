// 真实Common staging与两个保护口的有限原子边界；不将夹具准备器当S5永久业务证明。
#include "resident/storage/common_store.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Owner person(){return {Owner::Kind::actor,"p",{}};}
CommonState initial(){CommonState s;s.identity={"world",1,7,3};s.next_instance_id=2;s.wallets.set("cash",{"cash",person(),100,{},{}});s.inventories.set("bag",{"bag",person(),100,0,{{"lot:1","rice",{},5,person(),{}}},{}});return s;}
std::shared_ptr<CommonCatalog> catalog(bool freight=true,bool market=true){auto c=std::make_shared<CommonCatalog>();c->items.set("rice",{"rice",1,1,false,false});c->private_shop_freight_protection_enabled=freight;if(market)c->protected_market_inventory_ids={"bag"};return c;}
ResourceRecipe recipe(){return {{WalletDelta{"cash",-7,true},AddLot{"bag","rice",2,person(),{}}},{}};}
// 仅供纯准备结果观察；没有外部commit回调或析构写者。
struct Prepared final:PreparedFreightRecipe {Integer next=0;std::size_t prefix=0;Prepared(Integer n,std::size_t p):next(n),prefix(p){}};
struct Read final:ProtectedFreightResourceRead {
    const CommonStore* expected=nullptr;bool reject=false,null_success=false,probe_reentry=false;
    mutable int calls=0;mutable bool actual_staging=false,unpublished=false,finish_rejected=false,reentry_rejected=false;
    Outcome<std::unique_ptr<const PreparedFreightRecipe>> prepare_freight_recipe(const CommonStore& c,const ResourceExecutionDraft& e)const override{
        ++calls;if(&c!=expected)return {false,{},common_failure("WrongFreightCandidate").error};
        actual_staging=e.next_instance_before==c.state().next_instance_id&&e.next_instance_after==e.next_instance_before+1&&e.operations.size()==2&&e.operations[1].lot_flows.size()==1&&e.operations[1].lot_flows[0].destination_lot_id=="lot:"+std::to_string(e.next_instance_before);
        unpublished=e.operations[0].wallets[0].before->balance==c.wallet("cash")->balance&&e.operations[0].wallets[0].after.balance==c.wallet("cash")->balance-7&&e.operations[1].inventories[0].before->lots.size()==c.inventory("bag")->lots.size();
        if(probe_reentry){auto& mutable_c=const_cast<CommonStore&>(c);finish_rejected=!mutable_c.finish_protected_freight_resources().ok;auto nested=mutable_c.apply_resources(recipe());reentry_rejected=!nested.ok&&nested.error.original.code=="ResourceExecutionCaptureReentry";}
        if(reject)return {false,{},common_failure("FreightPrepareRejected").error};
        if(null_success)return {true,{}, {}};
        return {true,std::make_unique<Prepared>(e.next_instance_after,c.protected_freight_resource_drafts().size()),{}};
    }
};
// 原市场capture仍为最后一个可拒绝观察；只计本次是否真正接受，不模拟永久来源账。
struct Market final:ProtectedMarketResourceObserver {
    const CommonStore* expected=nullptr;bool reject=false;int accepted=0,calls=0;
    bool unpublished=false,freight_not_published=false;std::size_t expected_prefix=0;
    Status inspect_market_recipe(const CommonStore& c,const ResourceRecipe&)const override{return &c==expected?common_success():common_failure("WrongMarketCandidate");}
    Status capture_market_recipe(const CommonStore& c,const ResourceRecipe&,const OrderedTable<InventoryView>& staged,Integer next)override{
        ++calls;unpublished=staged.find("bag")&&staged.find("bag")->lots.size()==c.inventory("bag")->lots.size()+1&&next==c.state().next_instance_id+1;
        freight_not_published=c.protected_freight_resource_drafts().size()==expected_prefix;
        if(reject)return common_failure("MarketCaptureRejected");++accepted;return common_success();
    }
};
// 原操作覆盖行必须保留；该纯读准备器不作S5业务许可判断。
struct AcceptExecution final:ProtectedFreightResourceRead {
    Outcome<std::unique_ptr<const PreparedFreightRecipe>> prepare_freight_recipe(const CommonStore& c,const ResourceExecutionDraft& e)const override{return {true,std::make_unique<Prepared>(e.next_instance_after,c.protected_freight_resource_drafts().size()),{}};}
};
struct Fixture {
 std::shared_ptr<CommonCatalog> cat=catalog();CommonStore c{initial(),cat};Read read;Market market;
 Fixture(){read.expected=&c;market.expected=&c;c.bind_protected_market_resources(&market);}
 void begin(){check(c.begin_protected_freight_resources(read).ok,"begin exact candidate freight capture");}
};
bool same(const BoundaryValue& before,const CommonStore& c){return compare_boundary(before,encode_common_state(c.state())).exact();}
}
int main(){try{
 Fixture f;const auto before=encode_common_state(f.c.state());
 auto missing=f.c.apply_resources(recipe());check(!missing.ok&&missing.error.original.code=="ProtectedFreightGuardRequired"&&f.market.calls==0&&same(before,f.c),"missing selected guard rejects before either publication");
 f.begin();const auto capture=f.c.protected_freight_capture_identity();check(capture!=nullptr&&!f.c.begin_protected_freight_resources(f.read).ok,"capture identity real and no replacement");
 check(f.c.begin_resource_execution_capture().ok&&f.c.begin_wallet_debit_observation("cash").ok,"original captures coexist");
 f.read.reject=true;auto denied=f.c.apply_resources(recipe());check(!denied.ok&&denied.error.original.code=="FreightPrepareRejected"&&f.market.calls==0,"freight refusal occurs before market accepts");
 check(f.read.actual_staging&&f.read.unpublished&&same(before,f.c)&&f.c.protected_freight_resource_drafts().empty()&&f.c.resource_execution_drafts().empty()&&f.c.observed_wallet_debits()==0,"prepare failure leaves all Common bytes and both drafts untouched");
 f.read.reject=false;f.read.null_success=true;check(!f.c.apply_resources(recipe()).ok&&f.market.accepted==0&&same(before,f.c),"empty owned preparation is not acceptance");
 f.read.null_success=false;f.market.reject=true;denied=f.c.apply_resources(recipe());check(!denied.ok&&denied.error.original.code=="MarketCaptureRejected"&&f.market.accepted==0,"original market refusal remains authoritative");
 check(f.market.unpublished&&f.market.freight_not_published&&same(before,f.c)&&f.c.protected_freight_resource_drafts().empty()&&f.c.resource_execution_drafts().empty()&&f.c.observed_wallet_debits()==0,"market refusal discards prepared freight without publication");
 f.market.reject=false;f.read.probe_reentry=true;check(f.c.apply_resources(recipe()).ok,"real resource recipe succeeds");
 check(f.read.finish_rejected&&f.read.reentry_rejected,"prepare cannot drain its capture or reenter original primitive");
 check(f.c.wallet("cash")->balance==93&&f.c.state().next_instance_id==3&&f.c.inventory("bag")->lots.size()==2&&f.market.accepted==1&&f.c.observed_wallet_debits()==7,"same original money and lot result after both checks");
 check(f.c.protected_freight_resource_drafts().size()==1&&f.c.resource_execution_drafts().size()==1,"each successful recipe publishes both actual Common rows");
 const auto& first=f.c.protected_freight_resource_drafts()[0];const auto first_id=first.identity;
 check(first.capture==capture&&first.identity&&first.recipe_index==0&&first.execution.next_instance_before==2&&first.execution.next_instance_after==3,"Common seals exact capture index and real ID interval");
 const auto* ready=dynamic_cast<const Prepared*>(first.prepared.get());check(ready&&ready->next==3&&ready->prefix==0,"owned preparation is readable only with the successful row");
 const auto prefix_bytes=encode_common_state(f.c.state());const auto version=f.c.read_version({ResourceKey::Kind::inventory,"bag"});
 auto sp=f.c.begin_savepoint();check(sp.ok&&!f.c.finish_protected_freight_resources().ok,"original savepoint prohibits early capture removal");
 f.market.expected_prefix=1;check(f.c.apply_resources(recipe()).ok,"real second recipe uses successful prefix");
 const auto rolled_id=f.c.protected_freight_resource_drafts()[1].identity;
 check(f.c.protected_freight_resource_drafts()[1].recipe_index==1&&dynamic_cast<const Prepared*>(f.c.protected_freight_resource_drafts()[1].prepared.get())->prefix==1,"pure reader can fold prior actual rows");
 check(f.c.rollback_savepoint(sp.value).ok&&same(prefix_bytes,f.c)&&f.c.read_version({ResourceKey::Kind::inventory,"bag"}).version==version.version&&f.c.observed_wallet_debits()==7,"original savepoint restores complete bytes read version and gross debits");
 check(f.c.protected_freight_resource_drafts().size()==1&&f.c.protected_freight_resource_drafts()[0].identity==first_id&&f.c.resource_execution_drafts().size()==1,"rollback restores both exact owned prefixes");
 check(f.c.apply_resources(recipe()).ok&&f.c.protected_freight_resource_drafts()[1].identity!=rolled_id,"rollback redo same index and value gets a fresh actual recipe identity");
 sp=f.c.begin_savepoint();f.market.expected_prefix=2;check(f.c.apply_resources(recipe()).ok&&f.c.commit_savepoint(sp.value).ok&&f.c.protected_freight_resource_drafts().size()==3,"savepoint commit retains successful owned result");
 const int count=f.read.calls;auto broken=recipe();broken.operations.emplace_back(ConsumeLots{"bag","rice",999,{person()},{}});const auto final_bytes=encode_common_state(f.c.state());check(!f.c.apply_resources(broken).ok&&f.read.calls==count&&same(final_bytes,f.c)&&f.c.protected_freight_resource_drafts().size()==3,"late staging failure never calls pure prepare or leaves a row");
 CommonStore copy(f.c);copy.bind_protected_market_resources(&f.market);auto result=copy.apply_resources(recipe());check(!result.ok&&result.error.original.code=="ProtectedFreightGuardRequired"&&copy.protected_freight_resource_drafts().empty()&&!copy.protected_freight_capture_identity(),"copy preserves static protection and drops borrowed guard and proof");
 CommonStore assigned(initial(),f.cat);assigned=f.c;check(!assigned.protected_freight_capture_identity()&&!assigned.apply_resources(recipe()).ok,"copy assignment cannot inherit capture");
 auto delivered=f.c.finish_protected_freight_resources();check(delivered.ok&&delivered.value.size()==3&&!f.c.protected_freight_capture_identity()&&f.c.protected_freight_resource_drafts().empty(),"outer caller receives only committed same-stack rows");
 check(!f.c.finish_protected_freight_resources().ok&&!f.c.apply_resources(recipe()).ok,"consumed capture cannot authorize another recipe");f.begin();check(f.c.protected_freight_capture_identity()!=capture,"reopen has a distinct capture identity");
 for(bool assign:{false,true}){
  Fixture moving;moving.begin();moving.c.begin_resource_execution_capture();const auto saved=encode_common_state(moving.c.state());auto point=moving.c.begin_savepoint();check(point.ok&&moving.c.apply_resources(recipe()).ok,"actual capture and savepoint before move");
  CommonStore target=assign?CommonStore(initial(),moving.cat):CommonStore(std::move(moving.c));if(assign)target=std::move(moving.c);
  check(!target.protected_freight_capture_identity()&&target.protected_freight_resource_drafts().empty()&&!moving.c.protected_freight_capture_identity(),"move drops both borrowed proofs");
  check(target.rollback_savepoint(point.value).ok&&same(saved,target),"moved original savepoint restores resources after temporary proof removal");
 }
 auto oldcat=catalog(false,false);CommonStore old(initial(),oldcat),plain(initial(),oldcat);Read unused;unused.expected=&old;check(!old.begin_protected_freight_resources(unused).ok,"unselected old profile cannot activate new capture");check(old.apply_resources(recipe()).ok&&plain.apply_resources(recipe()).ok&&same(encode_common_state(plain.state()),old)&&unused.calls==0,"old profile exact encoded bytes and no new service call");
 Fixture nomarket;nomarket.cat->protected_market_inventory_ids.clear();nomarket.begin();check(nomarket.c.apply_resources(recipe()).ok&&nomarket.market.calls==0&&nomarket.c.protected_freight_resource_drafts().size()==1,"freight does not require market profile on unrelated old inventory");
 auto allcat=catalog(true,false);CommonStore trace(initial(),allcat);AcceptExecution passthrough;check(trace.begin_protected_freight_resources(passthrough).ok,"actual per-operation trace begins");
 auto no_revision=trace.inventory("bag")->revision;
 check(trace.apply_resources({{WalletDelta{"cash",-2,true},WalletDelta{"cash",2,true},SetLotOwner{"bag","lot:1",person(),false},AdjustLots{"bag",{{"lot:1",1}},false},AdjustLots{"bag",{{"lot:1",-1}},false}},{}}).ok,"real net-zero and no-revision operations publish");
 const auto& e=trace.protected_freight_resource_drafts()[0].execution;
 check(e.operations.size()==5&&e.operations[0].wallets[0].after.balance==98&&e.operations[1].wallets[0].after.balance==100,"net zero does not erase intermediate wallet writes");
 check(trace.inventory("bag")->revision==no_revision&&trace.inventory("bag")->lots[0].quantity==5&&e.operations[2].inventories.size()==1&&e.operations[3].lot_flows[0].quantity==1&&e.operations[4].lot_flows[0].quantity==-1,"same Owner and restored quantity remain actual ordered operations");
 const ReservationRef reservation{ReservationRef::Kind::contract,"contract:9"};
 check(trace.apply_resources({{ReserveLots{"bag","rice",2,{person()},reservation},ReleaseReservationResources{reservation}},{}}).ok,"real reserved split and indirect release");
 const auto& reserved=trace.protected_freight_resource_drafts()[1].execution;
 check(reserved.operations[0].lot_flows[0].kind==ResourceLotFlowDraft::Kind::reserve_split&&reserved.operations[0].lot_flows[0].source_lot_id=="lot:1"&&reserved.operations[0].lot_flows[0].destination_lot_id=="lot:2"&&reserved.operations[1].inventories[0].after.lots[1].reserved_by.empty(),"pure prepare receives original split ID and real release after state");
 check(trace.resource_execution_drafts().empty(),"freight trace does not silently enable the independent resource capture");
 CommonStore assignment_target(initial(),allcat);check(assignment_target.begin_protected_freight_resources(passthrough).ok&&assignment_target.apply_resources(recipe()).ok,"assignment target has real prior prepared state");auto prior_identity=assignment_target.protected_freight_capture_identity();assignment_target=trace;check(!assignment_target.protected_freight_capture_identity()&&assignment_target.protected_freight_resource_drafts().empty()&&same(encode_common_state(trace.state()),assignment_target),"copy assignment discards prior capture and preserves exact assets");
 std::cout<<"PROTECTED_FREIGHT_PREPARE="<<checks<<" FAILURES=0 SCOPE=COMMON_ATOMIC_PREPARATION_ONLY\n";return 0;
}catch(const std::exception& e){std::cerr<<"checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
