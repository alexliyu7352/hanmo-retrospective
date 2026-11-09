// 中央self_profile路由的增量回归；只测空参数授权边界和同候选绑定，不重复叶投影数值用例。
#include "resident/kernel/live_view_queries.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/views/self_profile_view.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;Integer checks=0;
// Release保留实际断言，失败原因直接区分路由、拒绝或世界变动。
void check(bool value,const StableId& why){++checks;if(!value)throw std::runtime_error(why);}
// 构造原WorldBundle的最小typed输入；两个现存人和继任人各保自己的共同记录。
InitialWorldSeed seed(){
    InitialWorldSeed out;out.travel=std::make_shared<TravelState>();out.military=std::make_shared<MilitaryState>();
    out.interaction=std::make_shared<InteractionState>();out.social=std::make_shared<SocialState>();
    out.estate=std::make_shared<EstateState>();out.tasks=std::make_shared<TaskState>();out.family=std::make_shared<FamilyState>();
    out.personal=std::make_shared<PersonalState>();out.growth=std::make_shared<GrowthState>();
    auto& state=out.common;state.metadata.player_actor_id="actor.player";state.identity={WorldId{0xab},4,17,720};
    for(const auto* id:{"actor.player","actor.shen","actor:19"}){
        ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=91;actor.attributes={61,62,63,64,65};
        actor.birth_tick=-7200;if(actor.id=="actor:19"){actor.prototype_id="actor.player";actor.display_name="继任者";}
        state.actors.insert(id,std::move(actor));state.request_sequences.insert(id,8);
    }
    state.next_instance_id=20;state.rng.insert("world",RngStream{19,123,{}});return out;
}
// 所有路由分支连同失败都比对完整共同状态，包含tick、世界三元组、ID、序列及RNG。
Outcome<V> query(WorldBundle& world,const V& args){
    const auto before=encode_common_state(world.common.state());const auto result=query_live_player_view(world,"self_profile",args);
    check(compare_boundary(before,encode_common_state(world.common.state())).exact(),"route preserves all common state");return result;
}
// 有效路由必须逐值等于同一候选的生产叶结果，不能使用上次选中的人或创建模板。
void current(WorldBundle& world){
    const auto out=query(world,boundary::obj({}));const auto expected=self_profile_view(world,world.common.state().metadata.player_actor_id);
    check(out.ok&&expected.ok&&compare_boundary(out.value,expected.value).exact(),"route binds current player and actual source");
}
// 任意目标/来源参数都不可绕过空参数契约，失败不携带部分档案。
void reject(WorldBundle& world,const V& args,const StableId& code){
    const auto out=query(world,args);check(!out.ok&&out.error.original.code==code&&std::holds_alternative<std::monostate>(out.value.data),"closed route rejection: "+code);
}
}
// 测中央注册、无目标参数和实际继任/共同行动绑定；原151项叶测试不在这里执行。
int main(){
    auto common=std::make_shared<CommonCatalog>();auto growth=std::make_shared<GrowthCatalog>();growth->cultivation=true;
    common->actor_names.insert("actor.player",ActorNameDefinition{"name.player",{}, {}});common->texts.insert("name.player","程和");
    common->actor_names.insert("actor.shen",ActorNameDefinition{"name.shen",{}, {}});common->texts.insert("name.shen","沈安");
    auto catalog=std::make_shared<PreparedCatalog>();catalog->common=common;catalog->growth=growth;WorldBundle world(catalog,seed());
    current(world);
    for(const auto* actor:{"actor.player","actor.shen","actor:19","missing",""})reject(world,boundary::obj({{"actor_id",V{actor}}}),"UnsupportedResidentField");
    for(const auto* field:{"observer","source","world_id","revision"})reject(world,boundary::obj({{field,V{"private-target"}}}),"UnsupportedResidentField");
    for(const auto& args:{V{std::monostate{}},V{V::Array{}},V{Integer{0}}})reject(world,args,"InvalidResidentObject");
    auto& state=world.common.state();state.metadata.player_actor_id="actor.shen";current(world);
    state.metadata.player_actor_id="actor:19";state.identity.execution_epoch=5;state.identity.revision=18;state.identity.world_id[0]=0xcd;current(world);
    auto* heir=state.actors.find("actor:19");heir->action_id="action.shared";
    state.actions.insert("action.shared",ActionCoreView{"action.shared","travel","actor.shen",{"actor.shen","actor:19"},720,2});current(world);
    const auto active=query(world,boundary::obj({}));check(active.ok&&boundary::get_string(*active.value.find("status"),"action_kind","status")=="travel","route keeps participant activity");
    state.metadata.player_actor_id="missing";reject(world,boundary::obj({}),"InvalidSelfProfileSource");
    std::cout<<"self_profile_route: PASS checks="<<checks<<" (strict empty args, no foreign observer, current player/heir/source/shared action, no mutation)\n";
}
