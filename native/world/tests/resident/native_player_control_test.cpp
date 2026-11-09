// 定点验证已有成人接管只改变控制所有权；真实钱货、预留、技能、行动和NPC行程原件不丢失。
#include "resident/kernel/native_player_control.hpp"
#include "resident/domains/director/director_model.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>

using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
const StableId cheng="actor.study_caravanner",shen="actor.study_swordswoman";
// 每个失败独立输出，不能把测试程序完成或日志存在当成通过。
void check(bool value,const StableId& label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<"\n";}}
// 完整共同状态字节包含钱货预留、技能、行动、ID、时钟和随机，避免只检查几个余额字段。
CodecVisitor::Buffer common_bytes(CommonState state){CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::typed_resource_references;auto written=visit_common_checkpoint(writer,state);check(written.ok&&writer.finish().ok,"共同状态完整检查点可写："+written.error.original.code);return bytes;}
// 行旅完整字节同时保留未执行意图及真实在途参数，不以重新生成的默认值冒充原件。
CodecVisitor::Buffer travel_bytes(TravelState state){CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_travel_checkpoint(writer,state).ok&&writer.finish().ok,"行旅完整检查点可写");return bytes;}
// 构造同一候选中的两位成人和第三位NPC；选谁都不创建或复制原人物。
CommonStore fixture(const StableId& selected){
    CommonState state;state.metadata.player_actor_id=selected;state.next_instance_id=71;state.rng.insert("world",{17,29,{}});
    for(const auto& id:{cheng,shen,StableId{"actor.steward"}}){
        ActorRecord actor;actor.id=id;actor.alive=actor.available=true;actor.hp=100;actor.wallet_id="wallet:"+id;actor.inventory_id="bag:"+id;actor.location=SettlementLocation{"qingchuan","gate"};actor.birth_tick=-43*720;actor.skills.commerce=37;actor.skills.martial=29;
        WalletView wallet;wallet.id=actor.wallet_id;wallet.owner={Owner::Kind::actor,id,{}};wallet.balance=73;wallet.reserved.push_back({{ReservationRef::Kind::contract,"contract:real"},11});state.wallets.insert(wallet.id,wallet);
        InventoryView bag;bag.id=actor.inventory_id;bag.owner=wallet.owner;bag.capacity=20;bag.lots.push_back({"lot:"+id,"millet",{ReservationRef::Kind::contract,"contract:real"},4,wallet.owner,{}});state.inventories.insert(bag.id,bag);
        state.actors.insert(id,actor);state.request_sequences.insert(id,0);
    }
    return CommonStore(std::move(state),std::make_shared<CommonCatalog>());
}
// 绑定正式CommonStore与作者行旅定义；实际出行调度仍使用生产travel_schedule_proposals。
struct Services final:TravelServices {
    CommonStore& common;const TravelCatalog& catalog;terrain::Flags flags;
    Services(CommonStore& value,const TravelCatalog& source):common(value),catalog(source){}
    Outcome<Location> resolved_location(const StableId& id)const override{return common.resolved_location(id);}
    StableId party_id(const StableId&)const override{return {};}
    Integer party_extra_cost(const StableId&)const override{return 0;}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId&)const override{return {true,{},{}};}
    bool talking(const StableId&)const override{return false;}
    Outcome<bool> admission_allowed(const StableId&,const StableId&)const override{return {true,true,{}};}
    const terrain::Flags& terrain_flags()const override{return flags;}
    const TravelScheduleDefinition* schedule(const StableId& id)const override{return catalog.authored_plans.find(id);}
    const StableId& player_actor_id()const override{return common.state().metadata.player_actor_id;}
};
// 拒绝路径必须逐字节保持输入；不通过清空真实行动或重置时钟来令人物可选。
void rejected_unchanged(CommonState common,TravelState travel,const StableId& actor,const StableId& code){
    const auto before_common=common_bytes(common),before_travel=travel_bytes(travel);auto result=prepare_native_player_control(common,travel,actor);
    check(!result.ok&&result.error.original.code==code,"明确拒绝："+code);
    check(common_bytes(common)==before_common&&travel_bytes(travel)==before_travel,"失败完整保留共同状态与行旅："+code);
}
// 已计划但未出发的原件保留合法，真实调度只交未选人物，另一人物的意图不被清空。
void check_takeover(const StableId& selected){
    auto common=fixture(selected);TravelState travel;TravelCatalog catalog;catalog.settlements.insert("qingchuan",{"qingchuan","gate",{0,0}});catalog.settlements.insert("next",{"next","gate2",{1,0}});
    for(const auto& id:{cheng,shen}){travel.itineraries.insert(id,{0,0,false,{}});catalog.authored_plans.insert(id,{{"next"},2,0});}
    Services services(common,catalog);const auto before_common=common_bytes(common.state()),before_travel=travel_bytes(travel);
    check(prepare_native_player_control(common.state(),travel,selected).ok,"接受未启动原NPC计划："+selected);
    check(common_bytes(common.state())==before_common&&travel_bytes(travel)==before_travel,"成功零资金/物品/技能/ID/RNG/行程改写");
    check(validate_travel_domain(common,travel,catalog,services).ok,"保留作者行程满足正式行旅终态验证");
    auto due=travel_schedule_proposals(common,travel,catalog,services);const auto other=selected==cheng?shen:cheng;
    check(due.ok&&due.value.size()==1&&due.value[0].actor_id==other,"真实行旅调度只为未选者提出出发");
    check(common_bytes(common.state())==before_common&&travel_bytes(travel)==before_travel,"调度查询也不消费计划或世界");
    auto broken=common.state();broken.actors.find(selected)->available=false;rejected_unchanged(broken,travel,selected,"InvalidPlayableActor");
    broken=common.state();broken.actors.find(selected)->alive=false;rejected_unchanged(broken,travel,selected,"InvalidPlayableActor");
    broken=common.state();broken.actors.find(selected)->action_id="work:ongoing";broken.actions.insert("work:ongoing",{"work:ongoing","service",selected,{selected},0,3});rejected_unchanged(broken,travel,selected,"PlayableActorBusy");
    broken=common.state();broken.actions.insert("other:ongoing",{"other:ongoing","service",other,{other,selected},0,3});rejected_unchanged(broken,travel,selected,"PlayableActorBusy");
    broken=common.state();broken.actors.find(selected)->location=OnRouteLocation{"road","qingchuan",50};rejected_unchanged(broken,travel,selected,"PlayableActorTravelling");
    auto moving=travel;moving.itineraries.find(selected)->travelling=true;rejected_unchanged(common.state(),moving,selected,"PlayableActorTravelling");
    moving=travel;moving.itineraries.find(selected)->index=1;rejected_unchanged(common.state(),moving,selected,"InvalidPlayableItinerary");
    broken=common.state();broken.identity.tick=1;rejected_unchanged(broken,travel,selected,"NativePlayerControlRequiresInitialState");
    broken=common.state();broken.identity.revision=1;rejected_unchanged(broken,travel,selected,"NativePlayerControlRequiresInitialState");
    rejected_unchanged(common.state(),travel,other,"InvalidPlayableActor");
}
// 新policy恰好排除本人，其他NPC和默认policy保持原语义；完整冷读后仍不能获得自主所有权。
void check_director(const StableId& selected){
    auto common=fixture(selected);DirectorCatalog catalog;DirectorPolicy policy;policy.templates={DirectorTemplate::await_adjustment};
    for(const auto& id:{cheng,shen,StableId{"actor.steward"}})catalog.subjects.insert(id,policy);
    auto legacy=make_initial_director(common,catalog);check(!legacy.ok&&legacy.error.original.code=="InvalidDirectorSubject","旧policy仍拒玩家作为Director主体");
    catalog.exclude_selected_player=true;const auto common_before=common_bytes(common.state());auto initial=make_initial_director(common,catalog);check(initial.ok,"新policy接受原NPC成为玩家");if(!initial.ok)return;
    check(initial.value.schedules.size()==2&&!initial.value.schedules.find(selected)&&!initial.value.plans.find(selected),"只剥离本人的自主schedule/plan所有权");
    check(validate_director(common,initial.value,catalog).ok,"新policy精确主体集合合法");
    TaskState tasks;TaskRecord contract;contract.id="contract:real";contract.assignee=selected;contract.status="active";contract.deadline_tick=1;tasks.tasks.insert(contract.id,contract);
    auto due=director_due_subjects(common,initial.value,catalog,tasks,0);check(due.ok&&due.value.size()==2&&std::find(due.value.begin(),due.value.end(),selected)==due.value.end(),"本人真实未完合同不会再触发Director替玩家履约");
    auto plain_due=director_due_subjects(initial.value,catalog,0);check(plain_due.ok&&plain_due.value.size()==2&&std::find(plain_due.value.begin(),plain_due.value.end(),selected)==plain_due.value.end(),"不借Common的旧队列也无玩家复评");
    auto restored=decode_director_seed(encode_director_state(initial.value));check(restored.ok&&validate_director(common,restored.value,catalog).ok,"恢复同一选中玩家仍有精确自主排除");
    auto injected=initial.value;injected.schedules.insert(selected,{{SubjectKind::actor,selected},0,0,0,-1,{},{}});check(!validate_director(common,injected,catalog).ok,"拒绝冷档残留玩家schedule");
    injected=initial.value;FinitePlan plan;plan.id="plan:player";plan.subject={SubjectKind::actor,selected};plan.template_id="maintain_commitment";plan.commitment_ids={"contract:real"};injected.plans.insert(selected,plan);check(!validate_director(common,injected,catalog).ok,"拒绝冷档残留玩家plan，不能据此删除真实合同");
    injected=initial.value;injected.schedules.erase(selected==cheng?shen:cheng);check(!validate_director(common,injected,catalog).ok,"不能额外跳过其他NPC");
    check(common_bytes(common.state())==common_before&&tasks.tasks.find("contract:real")->assignee==selected&&tasks.tasks.find("contract:real")->status=="active","Director准备与查询不修改钱货或真实合同");
    catalog.exclude_selected_player=false;catalog.subjects.erase(selected);auto unchanged=make_initial_director(common,catalog);check(unchanged.ok&&validate_director(common,unchanged.value,catalog).ok&&unchanged.value.schedules.size()==2,"正常旧默认玩家保持原精确两NPC语义");
}
// 家族已验证原founder后借入稳定ID；只检验Director继承接缝，不代替完整收养和继位流程。
void check_succession(const StableId& founder){
    auto common=fixture(founder);DirectorCatalog catalog;catalog.exclude_selected_player=true;DirectorPolicy policy;policy.templates={DirectorTemplate::await_adjustment};
    for(const auto& id:{cheng,shen,StableId{"actor.steward"}})catalog.subjects.insert(id,policy);
    auto initial=make_initial_director(common,catalog);check(initial.ok,"继承前原玩家自主主体已排除");if(!initial.ok)return;
    ActorRecord heir;heir.id="actor:71";heir.alive=heir.available=true;heir.hp=100;heir.location=SettlementLocation{"qingchuan","gate"};common.state().actors.insert(heir.id,heir);
    check(common.set_player_actor(heir.id).ok,"生产共同控制入口切至生成继承人");common.state().identity.tick=721;common.state().identity.revision=8;
    const auto before=common_bytes(common.state());
    check(!validate_director(common,initial.value,catalog).ok,"继承后缺少受信founder仍不猜测缺失主体");
    check(validate_director(common,initial.value,catalog,nullptr,founder).ok,"原founder排除在继承后仍精确有效");
    auto restored=decode_director_seed(encode_director_state(initial.value));check(restored.ok&&validate_director(common,restored.value,catalog,nullptr,founder).ok,"冷读原Director原件后接受同一founder继承链");
    const auto other=founder==cheng?shen:cheng;
    for(const auto& id:{founder,heir.id}){
        auto injected=initial.value;injected.schedules.erase(other);injected.schedules.insert(id,{{SubjectKind::actor,id},0,0,0,-1,{},{}});
        check(!validate_director(common,injected,catalog,nullptr,founder).ok,"相同主体计数也拒绝原founder或现玩家schedule："+id);
        injected=initial.value;FinitePlan plan;plan.id="plan:injected";plan.subject={SubjectKind::actor,id};plan.template_id="await_adjustment";injected.plans.insert(id,plan);
        check(!validate_director(common,injected,catalog,nullptr,founder).ok,"拒绝原founder或现玩家plan："+id);
    }
    auto missing=initial.value;missing.schedules.erase(other);check(!validate_director(common,missing,catalog,nullptr,founder).ok,"继承不允许其他NPC缺schedule");
    check(!validate_director(common,initial.value,catalog,nullptr,"unknown.founder").ok,"拒绝未知founder参数");
    check(!validate_director(common,initial.value,catalog,nullptr,other).ok,"不可用另一现存NPC掩盖原founder缺省记录");
    TaskState tasks;auto due=director_due_subjects(common,initial.value,catalog,tasks,common.tick());check(due.ok&&due.value.size()==2&&std::find(due.value.begin(),due.value.end(),founder)==due.value.end()&&std::find(due.value.begin(),due.value.end(),heir.id)==due.value.end(),"继承后实际Director队列不纳入原founder或现玩家");
    check(common_bytes(common.state())==before,"继承校验与队列查询不改共同世界");
    catalog.exclude_selected_player=false;catalog.subjects.erase(founder);check(validate_director(common,initial.value,catalog,nullptr,"ignored.legacy.value").ok,"旧policy完全忽略新增founder借参");
}
}
// 运行两名作者成人的对称接管与拒绝边界，不代替中央新局和物理存读整合门。
int main(int argc,char** argv){const bool succession_only=argc==2&&StableId{argv[1]}=="--succession";for(const auto& id:{cheng,shen}){if(!succession_only){check_takeover(id);check_director(id);}check_succession(id);}std::cout<<"NATIVE_PLAYER_CONTROL_CHECKS="<<checks<<" FAILURES="<<failures<<"\n";return failures==0?0:1;}
