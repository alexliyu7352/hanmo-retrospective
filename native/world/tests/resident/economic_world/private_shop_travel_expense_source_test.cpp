// 运输来源前置反例：只调用原Travel/Common写者，尚不证明新商铺运费账或完整World可用。
#include "resident/domains/travel/travel.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 断言记录实际边界；准备失败立即终止该夹具，避免后续空指针掩盖原红。
void check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
void require(bool value,const char* label){check(value,label);if(!value)throw std::runtime_error(label);}
// 测试服务只提供原旅行必需的只读能力；无军队/同行额外费用，不替原钱包执行收费。
struct Services final:TravelServices {
    CommonStore& common;terrain::Flags flags;
    explicit Services(CommonStore& c):common(c){}
    Outcome<Location> resolved_location(const StableId& id)const override{return common.resolved_location(id);}
    StableId party_id(const StableId&)const override{return {};}
    Integer party_extra_cost(const StableId&)const override{return 0;}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId&)const override{return {true,std::nullopt,{}};}
    bool talking(const StableId&)const override{return false;}
    Outcome<bool> admission_allowed(const StableId&,const StableId&)const override{return {true,true,{}};}
    const terrain::Flags& terrain_flags()const override{return flags;}
    const TravelScheduleDefinition* schedule(const StableId&)const override{return nullptr;}
    const StableId& player_actor_id()const override{return common.state().metadata.player_actor_id;}
};
// 有限双向道路分别需要三/两半日，原真实交会会在已付step中途截停。
struct Fixture {
    std::shared_ptr<CommonCatalog> common_catalog=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},common_catalog};TravelCatalog catalog;TravelState travel;Services services{common};
    Fixture(){
        catalog.settlements.insert("A",{"A","A.exit",{0,0}});catalog.settlements.insert("B",{"B","B.exit",{300,0}});
        catalog.places.insert("A.exit",{"A",{}});catalog.places.insert("B.exit",{"B",{}});
        catalog.roads.insert("AB",{"AB","A","B",3,7,2});catalog.roads.insert("BA",{"BA","B","A",2,11,3});
        person("carrier","A");person("other","B");common.state().metadata.player_actor_id="carrier";
    }
    // 只创建原初人物/本人钱包和行囊；实际行旅action随后由正式begin建立。
    void person(const StableId& id,const StableId& town){
        ActorRecord actor;actor.id=id;actor.wallet_id="wallet."+id;actor.inventory_id="bag."+id;actor.alive=actor.available=true;actor.location=SettlementLocation{town,town+".exit"};common.state().actors.insert(id,actor);
        WalletView wallet;wallet.id=actor.wallet_id;wallet.balance=100;wallet.owner={Owner::Kind::actor,id,{}};common.state().wallets.insert(wallet.id,wallet);
        InventoryView inventory;inventory.id=actor.inventory_id;inventory.capacity=100;inventory.owner=wallet.owner;common.state().inventories.insert(inventory.id,inventory);
    }
    // 原真实报价、同一个Common行动头和原begin，不把手填完成状态当作自然旅行。
    StableId begin(const StableId& actor,const StableId& target){
        auto quote=plan_travel(common,travel,catalog,services,actor,target);require(quote.ok,"original road quote");
        auto id=common.next_id("action");require(id.ok,"original action allocation");
        ActionCoreView action;action.id=id.value;action.actor_id=actor;action.kind="travel";action.participants={actor};action.start_tick=common.tick();action.remaining_ticks=quote.value.duration;
        ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;require(common.apply_action(insert).ok,"original action insert");
        ActorMutation occupy;occupy.actor_id=actor;occupy.action_id=id.value;require(common.apply_actor(occupy).ok,"original actor occupied");
        require(begin_travel(common,travel,catalog,id.value,quote.value).ok,"original travel begin");return id.value;
    }
    Integer funds(const StableId& actor)const{return common.wallet("wallet."+actor)->balance;}
    // 保持原旅行控制的prepare/apply两阶段；其中没有推进或付款。
    void control(const StableId& actor,const TravelCommandPayload& command){auto quote=prepare_travel_control(common,travel,catalog,services,actor,command);require(quote.ok,"original control prepare");require(apply_travel_control(common,travel,catalog,quote.value).ok,"original control apply");}
};
// 免费端点是明确的低层边界夹具；即使钱包已空，原收尾不产生第四笔费用。
void free_endpoint(){
    Fixture f;auto action=f.begin("carrier","B");
    // 此行专门构造旧合法OnRoute端点输入，不声称由前面begin自然走到终点。
    f.common.state().actors.find("carrier")->location=OnRouteLocation{"AB","A",300};f.common.state().wallets.find("wallet.carrier")->balance=0;
    const auto before=f.common.state().next_instance_id;auto done=step_travel(f.common,f.travel,f.catalog,f.services,action);
    require(done.ok,"endpoint finalization succeeds without funds");check(done.value.completed,"endpoint finalization completes");check(f.funds("carrier")==0,"endpoint finalization does not debit");
    check(f.common.actor("carrier")->fatigue==0&&f.common.state().next_instance_id==before,"endpoint no fatigue or payment fact allocated");
    auto place=std::get_if<SettlementLocation>(&f.common.actor("carrier")->location);check(place&&place->settlement_id=="B","endpoint reaches original destination");
}
// 暂停/等待/复行只有控制语义；真正移动才由原step收取一笔。
void control_without_fees(){
    Fixture f;auto action=f.begin("carrier","B");const auto next=f.common.state().next_instance_id;
    f.control("carrier",PauseTravelCommand{});check(f.funds("carrier")==100,"pause has no fee");auto paused=step_travel(f.common,f.travel,f.catalog,f.services,action);check(!paused.ok&&paused.error.original.code=="TravelPaused","paused step refuses");
    f.control("carrier",ResumeTravelCommand{});check(f.funds("carrier")==100,"resume has no fee");
    f.control("carrier",TravelWaitCommand{});check(f.funds("carrier")==100&&f.common.tick()==0,"wait control itself has no caller fee or clock");
    f.control("carrier",ResumeTravelCommand{});check(f.common.state().next_instance_id==next,"controls create no original payment facts");
    auto step=step_travel(f.common,f.travel,f.catalog,f.services,action);require(step.ok,"actual paid road step");check(f.funds("carrier")==93,"one actual movement costs seven");
    const auto paid=f.funds("carrier");f.control("carrier",ReturnTravelCommand{});check(f.funds("carrier")==paid,"return replan does not prepay or refund");
    check(f.travel.road_actions.find(action)->returning,"real return keeps original action and reverses direction");
}
// 原两人先收费再接敌截停，费用不按截短距离重算；交谈结束恢复旧pause也不收费。
void actual_contact_keeps_paid_step(){
    Fixture f;auto other=f.begin("other","A");require(step_travel(f.common,f.travel,f.catalog,f.services,other).ok,"other takes actual first step");++f.common.state().identity.tick;
    auto carrier=f.begin("carrier","B");auto hits=detect_road_contacts(f.common,f.travel,f.catalog,f.services);require(hits.ok&&hits.value.size()==1,"original contact geometry finds one crossing");
    const auto before_carrier=f.funds("carrier"),before_other=f.funds("other");
    require(step_travel(f.common,f.travel,f.catalog,f.services,carrier).ok,"carrier original paid step");require(step_travel(f.common,f.travel,f.catalog,f.services,other).ok,"other original paid step");
    const auto charged_carrier=f.funds("carrier"),charged_other=f.funds("other");check(charged_carrier==before_carrier-7&&charged_other==before_other-11,"both original real fees paid once");
    require(apply_contact_truncation(f.common,f.travel,hits.value.front(),"encounter.fixture",true).ok,"original contact truncates after payment");
    auto location=std::get_if<OnRouteLocation>(&f.common.actor("carrier")->location);require(location!=nullptr,"contact keeps carrier on original route");check(location->progress>0&&location->progress<100,"real contact cuts carrier step short");
    check(f.funds("carrier")==charged_carrier&&f.funds("other")==charged_other,"contact neither refunds nor repeats fees");
    const auto fatigue=f.common.actor("carrier")->fatigue;const auto remaining=f.common.action(carrier)->remaining_ticks;const auto instance=f.common.state().next_instance_id;const auto point=location->progress;
    require(resume_after_encounter(f.travel,carrier,false).ok,"original encounter resume");
    check(f.funds("carrier")==charged_carrier&&f.funds("other")==charged_other,"encounter resume has no fee");
    check(f.common.actor("carrier")->fatigue==fatigue&&f.common.action(carrier)->remaining_ticks==remaining&&f.common.state().next_instance_id==instance,"resume preserves paid fatigue action and allocation");
    check(std::get<OnRouteLocation>(f.common.actor("carrier")->location).progress==point,"resume does not replay movement");
    check(!f.travel.road_actions.find(carrier)->paused&&f.travel.road_actions.find(carrier)->encounter_id.empty(),"resume restores original pause state");
}
}
// 仅原费用边界的组件入口，不能当商铺运输来源模块或完整World验收。
int main(){try{free_endpoint();control_without_fees();actual_contact_keeps_paid_step();}catch(const std::exception& e){check(false,e.what());}std::cout<<"PRIVATE_SHOP_TRAVEL_SOURCE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
