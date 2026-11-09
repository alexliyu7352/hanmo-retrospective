// 本人档案的真实typed投影回归；覆盖当前人物、继任及共享行动，不冒充完整银行或GUI验收。
#include "resident/views/self_profile_view.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;using A=V::Array;
Integer checks=0;
// Release保留每项断言，并把失败条件直接交给调用方。
void check(bool value,const StableId& why){++checks;if(!value)throw std::runtime_error(why);}
// 只给已有共同字段不同的哨兵数值，不从原型或初始模板补当前值。
ActorRecord actor(const StableId& id,Integer value,Integer age){
    ActorRecord result;result.id=id;result.alive=true;result.available=true;result.hp=91;
    result.fatigue=12;result.injury=1;result.birth_tick=720-age*720;result.attributes={value,value+1,value+2,value+3,value+4};
    result.skills.infantry=100;result.skills.commerce=650;return result;
}
// 使用原WorldBundle构造器和真实领域类型，空领域不伪造第二份业务实现。
InitialWorldSeed seed(){
    InitialWorldSeed result;result.travel=std::make_shared<TravelState>();result.military=std::make_shared<MilitaryState>();
    result.interaction=std::make_shared<InteractionState>();result.social=std::make_shared<SocialState>();
    result.estate=std::make_shared<EstateState>();result.tasks=std::make_shared<TaskState>();result.family=std::make_shared<FamilyState>();
    result.personal=std::make_shared<PersonalState>();result.growth=std::make_shared<GrowthState>();
    auto& state=result.common;state.identity.tick=720;state.identity.revision=17;state.identity.execution_epoch=4;
    state.identity.world_id[0]=0xab;state.metadata.player_actor_id="actor.player";
    state.actors.insert("actor.player",actor("actor.player",61,23));state.actors.insert("actor.shen",actor("actor.shen",72,45));
    auto heir=actor("actor:19",34,19);heir.prototype_id="actor.player";heir.display_name="继任者";state.actors.insert(heir.id,heir);
    state.rng.insert("world",RngStream{19,123,{}});state.next_instance_id=20;state.request_sequences.insert("actor.player",8);
    state.roles.insert("role.self",RoleRecord{"actor.player",true,{}});
    state.roles.insert("role.other",RoleRecord{"actor.shen",true,{}});
    state.roles.insert("role.disabled",RoleRecord{"actor.player",false,{}});return result;
}
// 检查结果的白名单及原共同状态全值，保证连续查看不消耗tick、ID、指令序列或随机流。
V profile(WorldBundle& world,const StableId& id){
    const auto before=encode_common_state(world.common.state());const auto out=self_profile_view(world,id);
    check(out.ok,"current self profile succeeds");check(compare_boundary(before,encode_common_state(world.common.state())).exact(),"profile read preserves common state");
    check(boundary::order(out.value)==std::vector<StableId>({"kind","actor_id","display_name","age_years","attributes","skills","roles","status","source"}),"finite profile whitelist");
    check(boundary::get_string(out.value,"actor_id","profile")==id,"current actor identity");return out.value;
}
// 错误不得返回别人的部分数值，也不得写入原共同状态。
void reject(WorldBundle& world,const StableId& id,const StableId& code){
    const auto before=encode_common_state(world.common.state());const auto out=self_profile_view(world,id);
    check(!out.ok&&out.error.original.code==code&&std::holds_alternative<std::monostate>(out.value.data),"closed rejection: "+code);
    check(compare_boundary(before,encode_common_state(world.common.state())).exact(),"rejected read preserves common state");
}
}
// 当前角色/存档继任、无生日、能力关闭及畸形来源均经同一个生产函数。
int main(){
    auto common=std::make_shared<CommonCatalog>();auto growth=std::make_shared<GrowthCatalog>();growth->cultivation=true;
    common->actor_names.insert("actor.player",ActorNameDefinition{"name.player",{}, {}});
    common->actor_names.insert("actor.shen",ActorNameDefinition{"name.shen",{}, {}});
    common->texts.insert("name.player","程和");common->texts.insert("name.shen","沈安");
    for(const auto* id:{"role.self","role.other","role.disabled"}){RoleDefinition role;role.id=id;role.place_id="place.home";role.organization_id="org.real";common->roles.insert(id,role);}
    auto catalog=std::make_shared<PreparedCatalog>();catalog->common=common;catalog->growth=growth;WorldBundle world(catalog,seed());
    auto out=profile(world,"actor.player");
    check(boundary::get_string(out,"display_name","profile")=="程和"&&boundary::get_int(out,"age_years","profile")==23,"actual player name and age");
    const auto& attributes=*out.find("attributes")->as<A>();check(attributes.size()==5&&boundary::get_int(attributes[0],"value","attribute")==64,"five current attribute values");
    const auto& skills=*out.find("skills")->as<A>();check(skills.size()==10&&boundary::get_int(skills[0],"level","skill")==1&&boundary::get_int(skills[8],"level","skill")==3,"existing skill experience levels");
    check(out.find("roles")->as<A>()->size()==1&&boundary::get_string(out.find("roles")->as<A>()->front(),"role_id","role")=="role.self","only enabled owned roles");
    check(boundary::get_string(*out.find("source"),"world_id","source")=="ab000000000000000000000000000000"&&boundary::get_int(*out.find("source"),"revision","source")==17,"actual world source tuple");
    for(int i=0;i<20;++i)check(compare_boundary(out,profile(world,"actor.player")).exact(),"repeated stable read");
    for(const auto* other:{"actor.shen","missing",""})reject(world,other,"SelfProfileNotCurrentPlayer");
    auto& state=world.common.state();state.metadata.player_actor_id="actor.shen";out=profile(world,"actor.shen");
    check(boundary::get_string(out,"display_name","profile")=="沈安"&&boundary::get_int(out,"age_years","profile")==45&&boundary::get_int(out.find("attributes")->as<A>()->front(),"value","attribute")==75,"second actual actor keeps own facts");
    reject(world,"actor.player","SelfProfileNotCurrentPlayer");state.metadata.player_actor_id="actor:19";out=profile(world,"actor:19");
    check(boundary::get_string(out,"display_name","profile")=="继任者"&&boundary::get_int(out,"age_years","profile")==19&&boundary::get_int(out.find("attributes")->as<A>()->front(),"value","attribute")==37,"heir uses saved current values rather than prototype");
    check(out.find("roles")->as<A>()->empty(),"heir does not inherit template roles");
    auto* heir=state.actors.find("actor:19");heir->birth_tick.reset();out=profile(world,"actor:19");check(boundary::get_int(out,"age_years","profile")==-1,"unknown birthday explicit");
    heir->action_id="action.shared";state.actions.insert("action.shared",ActionCoreView{"action.shared","travel","actor.shen",{"actor.shen","actor:19"},0,2});out=profile(world,"actor:19");
    check(boundary::get_string(*out.find("status"),"action_kind","status")=="travel","shared action participant has actual status");
    state.actions.find("action.shared")->participants={"actor.shen"};reject(world,"actor:19","InvalidSelfProfileSource");heir->action_id.clear();
    heir->skills.infantry=1201;reject(world,"actor:19","InvalidSelfProfileSource");growth->cultivation=false;out=profile(world,"actor:19");check(out.find("skills")->as<A>()->empty(),"disabled cultivation reveals no skill table");
    heir->attributes.leadership=111;reject(world,"actor:19","InvalidSelfProfileSource");heir->attributes.leadership=37;
    state.roles.find("role.self")->actor_id="actor:19";common->roles.erase("role.self");reject(world,"actor:19","InvalidSelfProfileSource");
    std::cout<<"self_profile_view: PASS checks="<<checks<<" (actual player/heir, readonly state, bounded whitelist, enabled roles/skills, shared activity)\n";
}
