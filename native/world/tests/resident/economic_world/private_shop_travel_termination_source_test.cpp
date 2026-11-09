// 只测试原终止读的薄映射和坏边界；有限Naval原件夹具不冒完整战史或自然航行行为。
#include "resident/domains/commerce/freight/private_shop_travel_termination_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/naval_battle_state.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 记录实际谓词结果，准备失败停止夹具而不是继续伪造原来源。
void check(bool value,const char* message){++checks;if(!value){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
void require(bool value,const char* message){check(value,message);if(!value)throw std::runtime_error(message);}
// 直接构造仅供原query的最小原Navigation载荷；没有声称通过完整Military/Naval史。
struct Fixture {
    CommonStore common{CommonState{},std::make_shared<CommonCatalog>()};MilitaryState military;MilitaryCatalog catalog;NavalState naval;
    StableId action_id,source_id;
    Fixture(){
        ActorRecord actor;actor.id="carrier";actor.alive=actor.available=true;common.state().actors.insert(actor.id,actor);
        auto action=common.next_id("action");require(action.ok,"original action instance");action_id=action.value;
        auto voyage=common.next_id("naval_voyage");require(voyage.ok,"original voyage instance");require(common.append_fact({voyage.value,"naval_voyage","military_naval"}).ok,"original voyage header");
        auto step=common.next_id("naval_navigation");require(step.ok,"original navigation instance");source_id=step.value;require(common.append_fact({source_id,"naval_navigation_step","military_naval"}).ok,"original navigation header");
        NavalVoyageFact v;v.id=voyage.value;v.actor_id="carrier";v.action_id=action_id;naval.navigation.voyages.push_back(v);
        NavalNavigationStepFact s;s.id=source_id;s.voyage_source_id=v.id;s.tick=1;s.interrupted=true;s.movement.action_id=action_id;s.movement.actor_id="carrier";s.movement.source_tick=0;s.vessels.push_back({"ship",{},0,1,1,0,{}});naval.navigation.steps.push_back(s);common.state().identity.tick=1;
    }
    PrivateShopTravelMilitaryEndRequest request()const{return {PrivateShopTravelExpenseEndKind::naval_navigation,source_id,action_id,"carrier"};}
};
// 原Navigation精确source/action/actor和稳定钟都不可被任意理由或pending头代替。
void navigation_original_mapping(){
    Fixture f;auto request=f.request();auto original=read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,request);require(original.ok,"original Navigation query maps");
    check(original.value.tick==1&&original.value.original_reason=="NavalNavigationDisabled"&&original.value.battle_id.empty()&&original.value.casualty_fact_id.empty(),"only actual original navigation reason and tick returned");
    f.common.state().identity.tick=0;check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,request).ok,"preclock future original source rejected");f.common.state().identity.tick=1;
    check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,nullptr,request).ok,"absent Naval state does not mean free arrival");
    auto bad=request;bad.kind=PrivateShopTravelExpenseEndKind::step_completed;check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,bad).ok,"ordinary completed step cannot be military cause");
    bad=request;bad.kind=PrivateShopTravelExpenseEndKind::naval_round;check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,bad).ok,"navigation head cannot change into round cause");
    ActorRecord other;other.id="other";f.common.state().actors.insert(other.id,other);bad=request;bad.actor_id=other.id;check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,bad).ok,"another actual actor cannot claim original termination");
    f.naval.navigation.steps.front().interrupted=false;check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,request).ok,"noninterrupting original step cannot terminate travel");f.naval.navigation.steps.front().interrupted=true;
    ActionMutation active;active.kind=ActionMutation::Kind::insert;active.action.id=f.action_id;active.action.actor_id="carrier";active.action.kind="travel";active.action.participants={"carrier"};active.action.remaining_ticks=1;require(f.common.apply_action(active).ok,"bad unreleased original action fixture");check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,request).ok,"matching source before real release still rejected");
}
// 头本身不替原casualty/round载荷，不能造一个kind正确的Common壳就通过薄口。
void no_header_only_authority(){
    Fixture f;
    for(auto kind:{PrivateShopTravelExpenseEndKind::military_casualties,PrivateShopTravelExpenseEndKind::naval_round,PrivateShopTravelExpenseEndKind::naval_capture,PrivateShopTravelExpenseEndKind::naval_continue}){
        const StableId tag=kind==PrivateShopTravelExpenseEndKind::military_casualties?"military":kind==PrivateShopTravelExpenseEndKind::naval_round?"naval_round":kind==PrivateShopTravelExpenseEndKind::naval_capture?"naval_capture":"naval_continue";
        auto id=f.common.next_id(tag);require(id.ok,"original allocated empty header fixture");require(f.common.append_fact({id.value,tag=="military"?"military_casualties":tag,tag=="military"?"military":"military_naval"}).ok,"kind-exact original header fixture");
        check(!read_private_shop_travel_military_end(f.common,f.military,f.catalog,&f.naval,{kind,id.value,f.action_id,"carrier"}).ok,"header without unique original business payload rejected");
    }
}
}
int main(){try{navigation_original_mapping();no_header_only_authority();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_TERMINATION_SOURCE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
