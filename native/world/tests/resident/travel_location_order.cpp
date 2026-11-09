// 原野point更新必须保持legacy恢复后的键序；启程/据点抵达是真字典替换，不能混用模式。
#include "resident/domains/travel/travel.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 逐项核对位置值与键序，失败以非零退出。
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 最小独行fixture提供已明确为空的party/army/session输入，生产step仍走真实CommonStore。
struct Services final:TravelServices {
    CommonStore& common;terrain::Flags flags;StableId player="a";
    explicit Services(CommonStore& c):common(c){}
    Outcome<Location> resolved_location(const StableId& id)const override{return common.resolved_location(id);}
    StableId party_id(const StableId&)const override{return {};}
    Integer party_extra_cost(const StableId&)const override{return 0;}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId&)const override{return {true,std::nullopt,{}};}
    bool talking(const StableId&)const override{return false;}
    Outcome<bool> admission_allowed(const StableId&,const StableId&)const override{return {true,true,{}};}
    const terrain::Flags& terrain_flags()const override{return flags;}
    const TravelScheduleDefinition* schedule(const StableId&)const override{return nullptr;}
    const StableId& player_actor_id()const override{return player;}
};
}
// 同一真实行动先抵达自由点再返城，分别验证in-place与replacement的必要差别。
int main(){
    auto catalog=std::make_shared<CommonCatalog>();CommonState common_state;common_state.next_instance_id=2;
    ActorRecord actor;actor.id="a";actor.action_id="action:1";actor.wallet_id="wallet:a";actor.inventory_id="bag:a";actor.alive=actor.available=true;actor.location=WildernessLocation{"map.test","origin",{10,10}};actor.location_order={"kind","origin","point","map_id"};common_state.actors.insert(actor.id,actor);
    WalletView wallet;wallet.id="wallet:a";wallet.balance=1000;common_state.wallets.insert(wallet.id,wallet);InventoryView inventory;inventory.id="bag:a";inventory.capacity=100;common_state.inventories.insert(inventory.id,inventory);
    ActionCoreView action;action.id="action:1";action.kind="travel";action.actor_id="a";action.participants={"a"};action.remaining_ticks=2;common_state.actions.insert(action.id,action);
    CommonStore common(std::move(common_state),catalog);Services services(common);TravelCatalog travel_catalog;TravelState state;
    terrain::TerrainMap map;map.id="map.test";map.width=40;map.height=40;map.cell_size=20;map.cells.resize(4);map.budgets={{terrain::Mode::foot,100},{terrain::Mode::army,100},{terrain::Mode::boat,100}};map.money_per_tick=7;map.fatigue_per_tick=1;travel_catalog.world_map_id=map.id;travel_catalog.maps.insert(map.id,map);travel_catalog.settlements.insert("origin",{"origin","exit",{10,10}});travel_catalog.settlements.insert("other",{"other","other.exit",{10,10}});
    TravelQuote quote;quote.map_id=map.id;quote.points={{10,10},{30,10}};quote.destination=PointI{30,10};quote.origin="origin";quote.mode="foot";quote.budget=100;quote.duration=2;quote.payer_wallet_id="wallet:a";quote.money_per_tick=7;quote.fatigue_per_tick=1;
    check(begin_terrain_travel(common,state,action.id,quote).ok,"begin succeeds");check(common.state().actors.find("a")->location_order.empty(),"begin replaces original dictionary order");
    const FieldOrder restored_order={"kind","origin","point","map_id"};auto* restored=common.state().actors.find("a");restored->location_order=restored_order;std::get<WildernessLocation>(restored->location).origin="other";
    auto first=step_terrain_travel(common,state,travel_catalog,services,action.id);check(first.ok&&!first.value.completed,"first real half-day continues");++common.state().identity.tick;
    check(common.state().actors.find("a")->location_order==restored_order,"ordinary motion preserves restored key order");check(std::get<WildernessLocation>(common.actor("a")->location).origin=="other","point update preserves actual origin field");
    auto second=step_terrain_travel(common,state,travel_catalog,services,action.id);check(second.ok&&second.value.completed,"free-point second half-day completes");++common.state().identity.tick;
    check(common.state().actors.find("a")->location_order==restored_order&&std::get<WildernessLocation>(common.actor("a")->location).point.x==30,"free-point completion is still in-place");
    quote.points={{30,10},{10,10}};quote.destination=StableId{"origin"};check(begin_terrain_travel(common,state,action.id,quote).ok,"new return plan succeeds");check(common.state().actors.find("a")->location_order.empty(),"replan is true dictionary replacement");common.state().actors.find("a")->location_order=restored_order;
    auto returning=step_terrain_travel(common,state,travel_catalog,services,action.id);check(returning.ok&&!returning.value.completed,"return motion continues");++common.state().identity.tick;auto arrived=step_terrain_travel(common,state,travel_catalog,services,action.id);check(arrived.ok&&arrived.value.completed,"settlement arrival completes");
    check(common.state().actors.find("a")->location_order.empty()&&std::get<SettlementLocation>(common.actor("a")->location).place_id=="exit","settlement arrival replaces shape and key order");
    std::cout<<"TRAVEL_LOCATION_ORDER_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
