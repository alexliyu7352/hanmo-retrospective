// 仅对真实100人oracle中的军团行旅做B域切片核对；不重放或模拟军粮、军令和世界其他phase。
#include "resident/domains/travel/travel.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 失败输出具体字段，不把分域相等冒充完整世界相等。
void check(bool ok,const std::string& label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 真实fixture的任何解码错误都立即中止该测试，而非退成空军队。
template<class T> T take(Outcome<T> result){if(!result.ok)throw std::runtime_error(result.error.original.code+":"+result.error.original.path);return std::move(result.value);}
// 只读冻结文件，绝不改写原oracle。
BoundaryValue read(const std::string& path){std::ifstream input(path);return take(parse_boundary_json(std::string((std::istreambuf_iterator<char>(input)),{})));}
// 测试只机械投影原军队身份/成员/fit，不自行发粮、组军或删去军事人物。
struct ArmyInput { StableId id;ArmyTravelView view; };
class Services final:public TravelServices,public MembershipResolver {
public:
    CommonStore& common;const TravelCatalog& catalog;std::vector<ArmyInput> armies;terrain::Flags flags;
    Services(CommonStore& store,const TravelCatalog& prepared,const BoundaryValue& source):common(store),catalog(prepared){
        for(const auto& row:boundary::object(*source.find("flags"),"flags"))flags.emplace(row.first,boundary::boolean(row.second,"flag"));
        for(const auto& row:boundary::object(*source.find("armies"),"armies")){const auto& value=row.second;if(boundary::get_string(value,"status","army")!="active")continue;ArmyInput army;army.id=row.first;army.view.commander_actor_id=boundary::get_string(value,"commander_actor_id","army");army.view.wallet_id=boundary::get_string(value,"wallet_id","army");army.view.inventory_id=boundary::get_string(value,"inventory_id","army");army.view.member_actor_ids=boundary::strings(*value.find("member_actor_ids"),"members");for(const auto& id:boundary::strings(*value.find("unit_ids"),"units")){const auto& unit=boundary::field(*source.find("military_units"),id,"units");auto fit=boundary::strings(*unit.find("fit"),"fit");army.view.fit_actor_ids.insert(army.view.fit_actor_ids.end(),fit.begin(),fit.end());}armies.push_back(std::move(army));}common.bind_membership(this);
    }
    // 真实活动成员归属按原军队序查找，俘虏不在该行动投影内。
    const ArmyInput* army(const StableId& actor)const{for(const auto& group:armies)if(std::find(group.view.member_actor_ids.begin(),group.view.member_actor_ids.end(),actor)!=group.view.member_actor_ids.end())return &group;return nullptr;}
    const std::vector<StableId>* active_army_members(const StableId& actor)const override{auto* group=army(actor);return group?&group->view.member_actor_ids:nullptr;}
    const StableId* army_leader(const StableId& actor)const override{auto* group=army(actor);return group?&group->view.commander_actor_id:nullptr;}
    Outcome<Location> resolved_location(const StableId& actor)const override{return common.resolved_location(actor);}
    StableId party_id(const StableId& actor)const override{for(const auto& row:common.state().parties.rows())if(std::find(row.second.members.begin(),row.second.members.end(),actor)!=row.second.members.end())return row.first;return {};}
    Integer party_extra_cost(const StableId& actor)const override{const auto* party=common.state().parties.find(party_id(actor));Integer total=0;if(party)for(const auto& row:party->terms.rows())total+=row.second.cost_per_tick;return total;}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId& actor)const override{auto* group=army(actor);return {true,group?std::optional<ArmyTravelView>{group->view}:std::nullopt,{}};}
    // 下列流程不属于此切片；若被意外调用应显式失败，而非伪装成条件允许。
    bool talking(const StableId&)const override{throw std::logic_error("Army slice must not query dialogue scheduling");}
    Outcome<bool> admission_allowed(const StableId&,const StableId&)const override{return {false,false,common_failure("UnsupportedNativeCapability","army fixture admission",ErrorKind::unsupported_capability).error};}
    const terrain::Flags& terrain_flags()const override{return flags;}
    const TravelScheduleDefinition* schedule(const StableId& actor)const override{return catalog.authored_plans.find(actor);}
    const StableId& player_actor_id()const override{return common.state().metadata.player_actor_id;}
};
// 保全部值/标签/字典原序的精确差异断言。
void equal(const BoundaryValue& a,const BoundaryValue& b,const std::string& label){auto difference=compare_boundary(a,b);check(difference.exact(),label+(difference.value_difference?" value "+difference.value_difference->path:"")+(difference.order_difference?" order "+difference.order_difference->path:""));}
}
// 输入只接受实际scale100 oracle索引；每个army step从它自己的真实before边界独立开始。
int main(int argc,char** argv){if(argc!=2)return 2;try{
    const std::string root=argv[1];auto index=read(root+"/oracle-index.json"),wrapped=read(root+"/expanded-catalog.json");const auto& data=*wrapped.find("data");auto common_catalog=std::make_shared<CommonCatalog>(take(decode_common_catalog(data)));auto first=read(root+"/"+boundary::get_string(*index.find("initial_state"),"file","initial"));auto travel_catalog=take(decode_travel_catalog(data,boundary::get_string(first,"scenario_id","state")));Integer tested_steps=0;
    for(const auto& command:boundary::array(*index.find("commands"),"commands")){if(boundary::get_string(*command.find("command"),"kind","command")!="advance")continue;auto before=read(root+"/"+boundary::get_string(*command.find("before"),"file","before"));auto after=read(root+"/"+boundary::get_string(*command.find("after"),"file","after"));std::vector<StableId> army_actions;for(const auto& row:boundary::object(*before.find("actions"),"actions")){if(boundary::get_string(row.second,"kind","action")!="travel")continue;const auto actor=boundary::get_string(row.second,"actor_id","action");for(const auto& group:boundary::object(*before.find("armies"),"armies"))if(boundary::get_string(group.second,"status","army")=="active"&&boundary::get_string(group.second,"commander_actor_id","army")==actor)army_actions.push_back(row.first);}
        for(const auto& action_id:army_actions){auto common_source=before;common_source.find("results")->data=BoundaryValue::Dict{};CommonStore common(take(decode_common_seed(common_source,*common_catalog)),common_catalog);auto state=take(decode_travel_seed(before));Services services(common,travel_catalog,before);const auto action=*common.action(action_id);const auto params=*state.actions.find(action_id);check(params.mode=="army","actual action uses army terrain mode");const auto* group=services.army(action.actor_id);check(group&&params.payer_wallet_id==group->view.wallet_id,"payer is actual army public wallet");
            if(common.tick()==5){auto quote=plan_terrain_travel(common,state,travel_catalog,services,action.actor_id,params.destination);check(quote.ok&&quote.value.budget==1636&&quote.value.starting_load==59&&quote.value.duration==8,"actual post-supply army quote");if(quote.ok)check(quote.value.payer_wallet_id==params.payer_wallet_id,"quote never chooses commander private wallet");}
            const auto next_id=common.state().next_instance_id;const auto members=*common.members(action.actor_id);auto step=step_terrain_travel(common,state,travel_catalog,services,action_id);check(step.ok&&!step.value.completed,"actual army half-day remains in transit");if(!step.ok)continue;++tested_steps;auto exported=take(export_travel_state(state));equal(*exported.find("action_params")->find(action_id),*after.find("actions")->find(action_id)->find("params"),"army params tick "+std::to_string(common.tick()+1));auto shared=encode_common_state(common.state());equal(*shared.find("actors")->find(action.actor_id)->find("location"),*after.find("actors")->find(action.actor_id)->find("location"),"army location/order");equal(*shared.find("wallets")->find(params.payer_wallet_id),*after.find("wallets")->find(params.payer_wallet_id),"army public wallet");check(common.action(action_id)->remaining_ticks==boundary::get_int(*after.find("actions")->find(action_id),"remaining_ticks","action"),"army remaining time");for(const auto& member:members)check(common.actor(member)->fatigue==boundary::get_int(*after.find("actors")->find(member),"fatigue","actor"),"member fatigue "+member);check(common.state().next_instance_id==next_id,"army step allocates no phantom IDs");
        }
    }
    check(tested_steps==3,"actual100 journey covers army ticks 5 to 8");
}catch(const std::exception& e){check(false,e.what());}std::cout<<"ARMY_TRAVEL_ORACLE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
