// 市场观察口的共同原语边界：无服务默认拒绝，失败不写任何原资源，复制不借旧候选。
#include "resident/storage/common_store.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
// 该测试只证明Common调用时序，业务原因、真实差量和永久回执由Growth独立门核。
struct Observer final:ProtectedMarketResourceObserver {
    mutable int inspections=0;int captures=0,accepted=0;bool reject_inspection=false,reject_capture=false;const CommonStore* expected=nullptr;
    Status inspect_market_recipe(const CommonStore& c,const ResourceRecipe&)const override{++inspections;if(&c!=expected)return common_failure("WrongCandidate");return reject_inspection?common_failure("UnknownMarketWriter"):common_success();}
    Status capture_market_recipe(const CommonStore& c,const ResourceRecipe&,const OrderedTable<InventoryView>& staged,Integer next)override{
        ++captures;if(&c!=expected||c.wallet("cash")->balance!=100||c.inventory("market")->lots.size()!=1||c.state().next_instance_id!=2)return common_failure("PublicationTooEarly");
        const auto* inventory=staged.find("market");if(!inventory||inventory->lots.size()!=2||inventory->lots[1].id!="lot:2"||next!=3)return common_failure("StagingMissing");
        if(reject_capture)return common_failure("MarketCaptureRejected");++accepted;return common_success();
    }
};
CommonState initial(){CommonState s;s.next_instance_id=2;Owner org{Owner::Kind::organization,"guild",{}};WalletView wallet;wallet.id="cash";wallet.owner=org;wallet.balance=100;s.wallets.insert(wallet.id,wallet);InventoryView inventory;inventory.id="market";inventory.owner=org;inventory.capacity=100;inventory.lots.push_back({"lot:1","rice",{},5,org,{}});s.inventories.insert(inventory.id,inventory);return s;}
ResourceRecipe recipe(){Owner org{Owner::Kind::organization,"guild",{}};ResourceRecipe r;r.operations.emplace_back(WalletDelta{"cash",-7,true});r.operations.emplace_back(AddLot{"market","rice",2,org,{}});return r;}
}
int main(){try{
 auto catalog=std::make_shared<CommonCatalog>();catalog->protected_market_inventory_ids={"market"};catalog->items.insert("rice",{"rice",1,1,false,false});CommonStore c(initial(),catalog);auto before=encode_common_state(c.state());auto r=recipe();
 auto missing=c.apply_resources(r);check(!missing.ok&&missing.error.original.code=="ProtectedMarketGuardRequired","catalog protection cannot disappear with missing service");check(compare_boundary(before,encode_common_state(c.state())).exact(),"missing guard leaves complete Common unchanged");
 Observer observer;observer.expected=&c;c.bind_protected_market_resources(&observer);observer.reject_inspection=true;check(!c.apply_resources(r).ok&&observer.inspections==1&&observer.captures==0,"unknown writer refused before staging");
 observer.reject_inspection=false;observer.reject_capture=true;check(c.begin_wallet_debit_observation("cash").ok,"original spend observation begins");auto rejected=c.apply_resources(r);check(!rejected.ok&&rejected.error.original.code=="MarketCaptureRejected"&&observer.accepted==0,"actual staged delta can refuse without accepted draft");check(compare_boundary(before,encode_common_state(c.state())).exact()&&c.observed_wallet_debits()==0,"capture failure preserves wallet, lot, ID and gross debit");c.clear_wallet_debit_observation();
 observer.reject_capture=false;auto point=c.begin_savepoint();check(point.ok,"original NPC savepoint is compatible");check(c.apply_resources(r).ok&&observer.accepted==1,"same candidate original primitive publishes after capture");check(c.wallet("cash")->balance==93&&c.inventory("market")->lots.size()==2&&c.state().next_instance_id==3,"normal ordered resource result preserved");check(c.rollback_savepoint(point.value).ok&&compare_boundary(before,encode_common_state(c.state())).exact(),"original Common savepoint undoes real protected resource writes");
 const auto observed_calls=observer.inspections;CommonStore copied(c);auto copy_result=copied.apply_resources(r);check(!copy_result.ok&&copy_result.error.original.code=="ProtectedMarketGuardRequired"&&observer.inspections==observed_calls,"copy never reuses old candidate observer");CommonStore assigned(initial(),catalog);assigned=c;check(!assigned.apply_resources(r).ok,"copy assignment clears market borrowing");
 Observer move_source;move_source.expected=&copied;copied.bind_protected_market_resources(&move_source);CommonStore moved(std::move(copied));auto move_result=moved.apply_resources(r);check(!move_result.ok&&move_result.error.original.code=="ProtectedMarketGuardRequired"&&move_source.inspections==0,"move drops even a real source observer until same candidate rebinding");CommonStore move_assigned(initial(),catalog);Observer other;other.expected=&move_assigned;move_assigned.bind_protected_market_resources(&other);move_assigned=std::move(assigned);check(!move_assigned.apply_resources(r).ok&&other.inspections==0,"move assignment drops prior observer too");
 auto legacy_catalog=std::make_shared<CommonCatalog>(*catalog);legacy_catalog->protected_market_inventory_ids.clear();CommonStore legacy(initial(),legacy_catalog);Observer unused;unused.reject_inspection=true;legacy.bind_protected_market_resources(&unused);check(legacy.apply_resources(r).ok&&unused.inspections==0&&unused.captures==0,"unselected original profile has no new callback or source requirement");
 std::cout<<"PROTECTED_MARKET_RESOURCES="<<checks<<" FAILURES=0 SCOPE=COMMON_STAGING_ONLY\n";return 0;
}catch(const std::exception& e){std::cerr<<"checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
