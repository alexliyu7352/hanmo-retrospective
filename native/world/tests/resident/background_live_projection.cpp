// 原Main只读输入经过真实session及唯一组装叶；模板/活动负例为显式控制夹具，不作为可发布世界。
#include "resident/kernel/world_codec.hpp"
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/live_view_queries.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 所有源显式给定；生产错误与字段序差异保留而不以默认DTO掩盖。
V read(const std::string& path){std::ifstream f(path);assert(f.good());std::string bytes((std::istreambuf_iterator<char>(f)),{});auto value=parse_boundary_json(bytes);assert(value.ok);return std::move(value.value);}
V object(std::initializer_list<std::pair<std::string,V>> fields){return V{D{fields}};}
template<class T>T checked(Outcome<T> value){if(!value.ok){std::cerr<<value.error.original.code<<":"<<value.error.original.path<<":"<<value.error.command<<"\n";std::abort();}return std::move(value.value);}
void require(Status value){if(!value.ok){std::cerr<<value.error.original.code<<":"<<value.error.original.path<<"\n";std::abort();}}
void exact(const V& expected,const V& actual){auto diff=compare_boundary(expected,actual);if(!diff.exact()){auto failure=diff.value_difference?diff.value_difference:diff.order_difference;std::cerr<<failure->path<<":"<<failure->reason<<"\n";std::abort();}}
std::string string(const V& value,const std::string& key){return boundary::get_string(value,key,"test");}
bool same(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
LiveOperation request(const WorldIdentity& identity,const std::string& id,V args=object({})){return {LiveOperationKind::query,identity,object({{"query_id",V{id}},{"args",std::move(args)}})};}
V query(ResidentReplaySession& session,const std::string& id,V args=object({})){auto identity=checked(session.identity());auto value=checked(session.execute_live(request(identity,id,std::move(args))));assert(same(identity,value.source_identity)&&same(identity,value.current_identity));return std::move(value.result);}
// 改作者输入只用于验证冷三态映射和原先后短路，不把不存在模板渲染成成功。
void set(V& value,const std::string& key,V item){if(auto prior=value.find(key))*prior=std::move(item);else value.as<D>()->emplace_back(key,std::move(item));}
void mapping_edges(const V& source,const V& original){auto player=string(source,"player_actor_id");auto place=string(*source.find("actors")->find(player)->find("location"),"place_id");auto scenario=string(source,"scenario_id");auto map=string(*original.find("data")->find("scenarios")->find(scenario),"world_map_id");
    for(Integer test=0;test<10;++test){auto catalog=original;auto& current=*catalog.find("data")->find("places")->find(place);auto& selected=*catalog.find("data")->find("world_maps")->find(map);std::string error;ErrorKind kind=ErrorKind::rule;
        if(test==1){set(current,"scene_template_ids",object({{"world",V{std::string("template.present")}}}));error="UnsupportedNativeCapability";kind=ErrorKind::unsupported_capability;}
        if(test==2){set(current,"scene_template_ids",object({{"world",V{std::string{}}}}));error="InvalidSceneTemplateMapping";}
        if(test==3){set(current,"scene_template_ids",V{false});error="InvalidSceneTemplateMapping";}
        if(test==4)set(current,"scene_template_ids",object({{"city",V{std::string("ignored.other.view")}}}));
        if(test==5){set(selected,"scene_template_id",V{std::string("template.present")});error="UnsupportedNativeCapability";kind=ErrorKind::unsupported_capability;}
        if(test==6){set(selected,"scene_template_id",V{std::string{}});error="InvalidSceneTemplateMapping";}
        if(test==7){set(selected,"scene_template_id",V{false});error="InvalidSceneTemplateMapping";}
        if(test==8){set(current,"scene_template_ids",object({{"world",V{std::string("template.present")}}}));set(selected,"scene_template_id",V{false});error="UnsupportedNativeCapability";kind=ErrorKind::unsupported_capability;}
        if(test==9){set(current,"scene_template_ids",object({{"world",V{Integer{0}}}}));set(selected,"scene_template_id",V{std::string("template.present")});error="InvalidSceneTemplateMapping";}
        auto prepared=checked(prepare_catalog(catalog,source));WorldBundle world(prepared,checked(decode_world_seed(source,*prepared)));require(world.validate());auto before=checked(encode_world_checkpoint(world));auto result=query_live_player_view(world,"screen_context",object({}));if(error.empty()){assert(result.ok&&boundary::get_bool(result.value,"world_scene_legacy","screen"));}else assert(!result.ok&&result.error.original.code==error&&result.error.kind==kind);assert(before==checked(encode_world_checkpoint(world)));
    }
}
// 未准入的真实域对象/活动不能被无限制false读口掩盖；本人HUD也不能读取公柜的钱。
void source_edges(const V& state,const V& catalog){auto prepared=checked(prepare_catalog(catalog,state));WorldBundle base(prepared,checked(decode_world_seed(state,*prepared)));const auto player=base.common.state().metadata.player_actor_id;
    {WorldBundle w(base);w.military.battles.push_back({"controlled.personal","personal","active",{player}});auto result=query_live_player_view(w,"screen_context",object({}));assert(!result.ok&&result.error.kind==ErrorKind::unsupported_capability);}
    {WorldBundle w(base);w.military.sieges.push_back({"controlled.siege","siege","active",{player}});auto result=query_live_player_view(w,"screen_context",object({}));assert(!result.ok&&result.error.kind==ErrorKind::unsupported_capability);}
    {WorldBundle w(base);ActionCoreView action;action.id="controlled.study";action.kind="study_skill";action.actor_id=player;action.participants={player};action.remaining_ticks=1;ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;require(w.common.apply_action(insert));auto result=query_live_player_view(w,"screen_context",object({}));assert(!result.ok&&result.error.kind==ErrorKind::unsupported_capability);}
    {WorldBundle w(base);auto actor=w.common.state().actors.find(player);auto wallet=w.common.state().wallets.find(actor->wallet_id);wallet->owner.kind=Owner::Kind::organization;auto result=query_live_player_view(w,"hud_view",object({}));assert(!result.ok&&result.error.original.code=="InvalidHudViewSource");}
    auto invalid=query_live_player_view(base,"hud_view",object({{"personal_battle_id",V{std::string("missing.battle")}}}));assert(!invalid.ok&&invalid.error.original.code=="InvalidHudViewSource");
}
}
// 三个已存在的原GD状态：出发前、实际会面choice、实际100人到达，不运行新规模或后续世界。
int main(int argc,char** argv){assert(argc==3);auto oracle=read(argv[1]);std::string root=argv[2];Integer cases=0;for(auto& row:*oracle.as<A>()){auto path=[&](const std::string& key){auto value=string(row,key);return !value.empty()&&value.front()=='/'?value:root+"/"+value;};auto state=read(path("state"));auto catalog=read(path("catalog"));auto session=create_resident_replay_session();require(session->initialize(catalog,state));auto before=checked(session->checkpoint());auto full=checked(session->export_state());auto identity=checked(session->identity());exact(*row.find("hud"),query(*session,"hud_view"));exact(*row.find("screen"),query(*session,"screen_context"));exact(*row.find("sidebar")->find("location"),query(*session,"journey_location"));
        auto map=query(*session,"journey_map_position");assert(string(map,"world_map_id")==string(*row.find("map"),"world_map_id"));auto& wire=*map.find("observer_position")->as<A>();auto& shown=*row.find("map")->find("observer_position")->as<A>();assert(wire.size()==2&&shown.size()==2);for(std::size_t i=0;i<2;++i){assert(wire[i].as<Integer>());auto expected=shown[i].as<double>()?*shown[i].as<double>():static_cast<double>(*shown[i].as<Integer>());assert(static_cast<double>(*wire[i].as<Integer>())==expected);}
        auto route=query(*session,"journey_route",object({{"cursor",V{Integer{0}}}}));auto& overlay=*row.find("map")->find("travel_overlay");auto points=overlay.find("points")?*overlay.find("points"):V{A{}};exact(points,*route.find("points"));assert(boundary::get_int(route,"total_rows","route")==static_cast<Integer>(points.as<A>()->size())&&!route.find("next_cursor"));assert(boundary::get_bool(route,"active","route")==!overlay.as<D>()->empty());
        auto crossings=query(*session,"journey_crossings",object({{"cursor",V{Integer{0}}}}));auto& expected_flags=*row.find("map")->find("crossing_closed")->as<D>();auto& flags=*crossings.find("rows")->as<A>();assert(flags.size()==expected_flags.size());for(std::size_t i=0;i<flags.size();++i){assert(string(flags[i],"flag_id")==expected_flags[i].first);exact(expected_flags[i].second,*flags[i].find("closed"));}
        auto companions=query(*session,"journey_companions");assert(!boundary::get_bool(companions,"party_present","companions")&&companions.find("rows")->as<A>()->empty());
        if(row.find("sidebar")->find("encounter")->as<D>()->empty())exact(*row.find("sidebar")->find("travel"),query(*session,"journey_travel"));
        for(auto id:{"journey_location","journey_map_position","journey_travel","journey_route","journey_crossings","journey_companions","hud_view","screen_context"}){auto denied=session->execute_live(request(identity,id,object({{"actor_id",V{std::string("actor.blackwater.commander")}}})));assert(!denied.ok);}
        auto denied=session->execute_live(request(identity,"character_input",object({{"actor_id",V{std::string("actor.blackwater.commander")}}})));assert(!denied.ok&&denied.error.original.code=="InvalidPresentationActor");auto own=query(*session,"character_input",object({{"actor_id",V{string(state,"player_actor_id")}}}));assert(string(*own.find("actor_view"),"actor_id")==string(state,"player_actor_id")&&!own.find("appearance_view")->find("generation_seed"));
        auto unknown=session->execute_live(request(identity,"not.a.registered.query"));assert(!unknown.ok&&unknown.error.original.code=="UnsupportedLiveQuery"&&unknown.error.kind==ErrorKind::unsupported_capability);assert(before==checked(session->checkpoint())&&same(identity,checked(session->identity())));exact(full,checked(session->export_state()));if(cases==0){mapping_edges(state,catalog);source_edges(state,catalog);}++cases;
    }assert(cases==3);std::cout<<"background_live_projection: PASS (3 original Main projections through actual session; six B wires, selected character, personal HUD, checked screen inactivity, 10 cold template cases and explicit unsupported-source guards; zero read mutation)\n";}
