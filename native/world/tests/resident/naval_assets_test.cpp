// 船只纯域门只证明真实资源引用合同，不冒充生产World或完整水战验收。
#include "resident/domains/military/naval_assets.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// Release下也执行的有限断言，首错终止。
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
// 前置夹具失败保真实错误，不能当业务拒绝计绿。
template<class T>T take(Outcome<T> r){if(!r.ok)throw std::runtime_error(r.error.original.code+":"+r.error.original.path);return std::move(r.value);}
// 构造原Common Owner，不另定义海军产权枚举。
Owner owner(Owner::Kind kind,const StableId& id){Owner value;value.kind=kind;value.id=id;return value;}
struct Fixture {
 NavalAssetCatalog catalog;NavalAssetState state;NavalFormationObservation observed;NavalFormationRequest request;
 // 测试观察只代表域内输入，未冒充真实军职/人口/资产来源。
 Fixture(){
  NavalHullDefinition hull;hull.id="river_boat";hull.max_integrity=80;hull.minimum_crew=2;hull.person_capacity=4;hull.cargo_capacity=100;hull.minimum_naval_level=1;hull.movement=3;hull.travel_budget=3000;hull.waters={"river"};catalog.hulls={hull};
  for(int i=0;i<2;++i){auto suffix=std::to_string(i);NavalVesselRecord ship;ship.id="ship"+suffix;ship.hull_id=hull.id;ship.owner=owner(Owner::Kind::organization,"north");ship.cargo_inventory_id="hold"+suffix;ship.berth_id="north_dock";ship.integrity=80;ship.origin_source="author:river";state.vessels.push_back(ship);observed.inventories.push_back({ship.cargo_inventory_id,ship.owner,10});NavalObservedUnit unit;unit.id="unit"+suffix;unit.army_id="army:1";unit.commander_actor_id="captain"+suffix;unit.fit={"sailor"+suffix+"a","sailor"+suffix+"b"};observed.units.push_back(unit);observed.people.push_back({unit.commander_actor_id,"bag"+suffix,true,true,2});observed.inventories.push_back({"bag"+suffix,owner(Owner::Kind::actor,unit.commander_actor_id),5});NavalVesselAssignment binding;binding.vessel_id=ship.id;binding.unit_id=unit.id;request.assignments.push_back(binding);observed.army_unit_ids.push_back(unit.id);observed.army_member_ids.push_back(unit.commander_actor_id);}
  observed.actor_id="captain0";observed.army_id="army:1";observed.organization_id="north";observed.army_inventory_id="army_store";observed.berth_id="north_dock";observed.office_id="office.commander";observed.appointment_source="military:2";observed.formation_source="military:3";observed.tick=4;observed.army_active=true;observed.inventories.push_back({"army_store",owner(Owner::Kind::organization,"north"),60});request.army_id=observed.army_id;request.assignments[0].extra_inventory_ids={"army_store"};
 }
};
}
// 同一unit和公物只有唯一实船占用；失能仍浮水，绝不凭纯资产层改写人员生死。
int main(){try{
 Fixture f;auto q=take(preview_naval_formation(f.state,f.catalog,f.observed,f.request));check(q.loads.size()==2&&q.loads[0].people==3&&q.loads[0].cargo_weight==75&&q.loads[1].cargo_weight==15,"per vessel includes crew, captain, private bags and actual public cargo");
 auto bound=take(commit_naval_formation(f.state,f.catalog,f.observed,f.request,"fleet:4","naval_action:5"));check(bound.fleets.size()==1&&bound.vessels[0].fleet_id=="fleet:4","formation binds original vessels without inventing cargo or crew");check(f.state.fleets.empty()&&f.state.vessels[0].fleet_id.empty(),"pure candidate leaves source registry intact");check(!preview_naval_formation(bound,f.catalog,f.observed,f.request).ok,"same real vessels cannot join another fleet");
 auto bad=f.request;bad.assignments[1].unit_id=bad.assignments[0].unit_id;check(!preview_naval_formation(f.state,f.catalog,f.observed,bad).ok,"one army unit cannot power two vessels");bad=f.request;bad.assignments.pop_back();check(!preview_naval_formation(f.state,f.catalog,f.observed,bad).ok,"fleet cannot leave an unassigned army unit on land");
 auto busy=f.observed;busy.people[0].action_id="action:9";check(!preview_naval_formation(f.state,f.catalog,busy,f.request).ok,"already acting captain cannot also embark for a new fleet");busy=f.observed;busy.units[0].busy_source_id="battle:8";check(!preview_naval_formation(f.state,f.catalog,busy,f.request).ok,"engaged unit cannot simultaneously become vessel crew");check(!commit_naval_formation(f.state,f.catalog,f.observed,f.request,"fleet:0","naval_action:5").ok,"only canonical allocated positive fleet identity is accepted");
 auto state=f.state;state.vessels[0].integrity=0;auto rejected=preview_naval_formation(state,f.catalog,f.observed,f.request);check(!rejected.ok&&rejected.error.original.code=="NavalVesselDisabled","disabled afloat hull cannot propel a departing fleet");check(validate_naval_assets(state,f.catalog).ok,"zero integrity is valid disabled afloat state, not implicit sinking");
 auto cat=f.catalog;cat.hulls[0].person_capacity=2;check(!preview_naval_formation(f.state,cat,f.observed,f.request).ok,"captain counts toward people capacity");cat=f.catalog;cat.hulls[0].cargo_capacity=74;check(!preview_naval_formation(f.state,cat,f.observed,f.request).ok,"per ship overload cannot borrow another hull capacity");
 auto o=f.observed;o.units[0].wounded.push_back(o.units[0].fit.back());o.units[0].fit.pop_back();check(!preview_naval_formation(f.state,f.catalog,o,f.request).ok,"wounded still occupy space but do not satisfy working crew");o=f.observed;o.people[0].naval_level=0;check(!preview_naval_formation(f.state,f.catalog,o,f.request).ok,"real captain skill required");
 state=f.state;state.vessels[0].owner=owner(Owner::Kind::actor,"stranger");check(!preview_naval_formation(state,f.catalog,f.observed,f.request).ok,"military office cannot appropriate private stranger ship");state=f.state;state.vessels[0].owner=owner(Owner::Kind::actor,"captain0");auto owned=f.observed;owned.inventories[0].owner=state.vessels[0].owner;check(preview_naval_formation(state,f.catalog,owned,f.request).ok,"commander can explicitly use own hull without changing title");
 bad=f.request;bad.assignments[1].extra_inventory_ids={"army_store"};check(!preview_naval_formation(f.state,f.catalog,f.observed,bad).ok,"same real cargo container cannot ride two ships");bad=f.request;bad.assignments[0].extra_inventory_ids.clear();check(!preview_naval_formation(f.state,f.catalog,f.observed,bad).ok,"public supplies cannot vanish from manifest");
 o=f.observed;o.prisoner_ids={"prisoner"};bad=f.request;check(!preview_naval_formation(f.state,f.catalog,o,bad).ok,"actual prisoners need explicit embarkation space");bad.assignments[0].prisoner_ids={"prisoner"};check(take(preview_naval_formation(f.state,f.catalog,o,bad)).loads[0].people==4,"actual prisoners count once without new population account");
 o.people.push_back({"prisoner","prisoner_bag",true,false,0});o.inventories.push_back({"prisoner_bag",owner(Owner::Kind::actor,"prisoner"),3});check(take(preview_naval_formation(f.state,f.catalog,o,bad)).loads[0].cargo_weight==78,"unavailable actual captive still embarks with own private bag");
 auto duplicate=f.observed;duplicate.inventories.push_back(duplicate.inventories.front());check(!preview_naval_formation(f.state,f.catalog,duplicate,f.request).ok,"ambiguous duplicate observed inventory is not a valid proof");
 cat=f.catalog;cat.hulls[0].minimum_naval_level=5;check(!validate_naval_assets(f.state,cat).ok,"hull skill demand must remain within original level0 to4");
 state=bound;auto second=state.fleets[0];second.id="fleet:6";second.assignments.erase(second.assignments.begin());state.fleets[0].assignments.pop_back();state.fleets.push_back(second);state.vessels[1].fleet_id=second.id;check(!validate_naval_assets(state,f.catalog).ok,"cold army cannot be split into two independently sailing fleet occupancies");
 state=bound;state.vessels[0].fleet_id.clear();check(!validate_naval_assets(state,f.catalog).ok,"cold registry rejects dangling crew/vessel assignment");cat=f.catalog;cat.profile="sinking_v1";check(!validate_naval_assets(f.state,cat).ok,"unsupported sinking result/profile fails explicitly");
 std::cout<<"NAVAL_ASSET_CHECKS="<<checks<<" FAILURES=0 SCOPE=PURE_DOMAIN\n";return 0;
 }catch(const std::exception& e){std::cerr<<"NAVAL_ASSET_FAIL checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
