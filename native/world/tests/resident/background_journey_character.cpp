// 真A人物投影上的C选择门；原GD人物DTO逐项比对，控制party/speaker夹具只验证读取资格，不发布合成世界。
#include "resident/kernel/world_codec.hpp"
#include "resident/views/journey_character.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
// 原证据读失败或生产错误都立即中止，不以空人物兜底。
V read_file(const std::string& path){std::ifstream file(path);assert(file.good());std::string bytes((std::istreambuf_iterator<char>(file)),{});auto parsed=parse_boundary_json(bytes);assert(parsed.ok);return std::move(parsed.value);}
template<class T>T checked(Outcome<T> result){if(!result.ok){std::cerr<<result.error.original.code<<":"<<result.error.original.path<<"\n";std::abort();}return std::move(result.value);}
void require(Status result){if(!result.ok){std::cerr<<result.error.original.code<<":"<<result.error.original.path<<"\n";std::abort();}}
void exact(const V& expected,const V& actual){auto diff=compare_boundary(expected,actual);if(!diff.exact()){auto failure=diff.value_difference?diff.value_difference:diff.order_difference;std::cerr<<failure->path<<":"<<failure->reason<<"\n";std::abort();}}
std::string string(const V& value,const std::string& key){return boundary::get_string(value,key,"test");}
// 记录端口调用但委托真A实现；未选择人物必须在首次记录前就被拒绝。
class ObservedCharacter final:public CharacterPresentationRead {
public:
    explicit ObservedCharacter(const CommonCharacterPresentation& source):source_(source){}
    Outcome<CharacterPresentationView> character(const StableId& id)const override{calls.push_back(id);return source_.character(id);}
    mutable std::vector<StableId> calls;
private:const CommonCharacterPresentation& source_;
};
// 逐层核白名单没有私密来源、资产或整人物子树；类型与原GD键序另由exact比较。
void no_private_fields(const V& value){if(auto object=value.as<V::Dict>()){for(auto& row:*object){for(auto key:{"generation_seed","parent_sources","source","wallet_id","inventory_id","skills","combat","knowledge","attributes","location"})assert(row.first!=key);no_private_fields(row.second);}}else if(auto array=value.as<V::Array>())for(auto& row:*array)no_private_fields(row);}
// 成功只允许一次指定人物读取，结果经A唯一encoder与原GD精确对照。
void allowed(const CommonState& common,const InteractionState& interaction,const InteractionCatalog& catalog,const CommonCatalog& common_catalog,const StableId& id,const V& expected){CommonCharacterPresentation actual(common,common_catalog);ObservedCharacter observed(actual);auto view=checked(query_selected_character(common,interaction,catalog,observed,id));assert(observed.calls==std::vector<StableId>{id});auto encoded=checked(encode_character_presentation(view));exact(expected,encoded);no_private_fields(encoded);view.actor_view.display_name="display-only mutation";view.appearance_view.attire_style="display-only mutation";exact(expected,checked(encode_character_presentation(checked(query_selected_character(common,interaction,catalog,actual,id)))));}
void denied(const CommonState& common,const InteractionState& interaction,const InteractionCatalog& catalog,const CommonCatalog& common_catalog,const StableId& id){CommonCharacterPresentation actual(common,common_catalog);ObservedCharacter observed(actual);auto result=query_selected_character(common,interaction,catalog,observed,id);assert(!result.ok&&result.error.original.code=="InvalidPresentationActor"&&observed.calls.empty());}
// 人为增补的party仅供过滤单测；不声称未执行的join_party动作或同军名册就是合法同行。
void selection_edges(const WorldBundle& world,const V& expected){auto common=world.common.state();auto interaction=world.interaction;auto catalog=*world.catalog->interaction;const auto player=common.metadata.player_actor_id;assert(!interaction.interactions.empty()&&!interaction.encounters.empty());auto original_interaction=interaction;interaction.encounters.clear();interaction.interactions.clear();common.parties={};PartyRecord first;first.id="controlled.party.first";first.leader_id=player;first.members={player,"actor.bandit"};common.parties.insert(first.id,first);PartyRecord second;second.id="controlled.party.second";second.leader_id=player;second.members={player,"actor.blackwater.commander"};common.parties.insert(second.id,second);
    allowed(common,interaction,catalog,*world.catalog->common,"actor.bandit",*expected.find("actor.bandit"));denied(common,interaction,catalog,*world.catalog->common,"actor.blackwater.commander");
    // 同一party遇到真实原会面就不能继续提供同行画像，但原peer仍可见。
    interaction=original_interaction;denied(common,interaction,catalog,*world.catalog->common,"actor.bandit");allowed(common,interaction,catalog,*world.catalog->common,"actor.walker",*expected.find("actor.walker"));
    interaction.encounters.clear();interaction.interactions.clear();common.parties.find(first.id)->leader_id="actor.bandit";denied(common,interaction,catalog,*world.catalog->common,"actor.bandit");allowed(common,interaction,catalog,*world.catalog->common,player,*expected.find(player));
    common.parties={};interaction=original_interaction;auto& session=interaction.interactions.front();DialogueNode* node=nullptr;for(auto& dialogue:catalog.dialogues)if(dialogue.id==session.dialogue_id)for(auto& value:dialogue.nodes)if(value.id==session.node_id)node=&value;assert(node);node->speaker="actor.blackwater.commander";session.phase="choice";denied(common,interaction,catalog,*world.catalog->common,"actor.blackwater.commander");session.phase="say";allowed(common,interaction,catalog,*world.catalog->common,"actor.blackwater.commander",*expected.find("actor.blackwater.commander"));
    // 其它人的当前会话不能成为当前玩家的可见许可。
    session.actor_id="actor.bandit";denied(common,interaction,catalog,*world.catalog->common,"actor.blackwater.commander");
    // 选中的真实人物投影错误原样传播，不能从空名字猜成合法身份。
    auto* actor=common.actors.find(player);actor->prototype_id="controlled.missing.prototype";CommonCharacterPresentation actual(common,*world.catalog->common);ObservedCharacter observed(actual);auto invalid=query_selected_character(common,interaction,catalog,observed,player);assert(!invalid.ok&&invalid.error.original.code=="InvalidPresentationActor"&&observed.calls==std::vector<StableId>{player});
}
}
// 原输入先由完整typed世界校验，再测有限读取；全导出/检查点用于证据，不是热查询实现。
int main(int argc,char** argv){assert(argc==3);auto rows=read_file(argv[1]);std::string root=argv[2];Integer cases=0,selected=0;for(auto& row:*rows.as<V::Array>()){auto path=[&](const std::string& key){auto value=string(row,key);return !value.empty()&&value.front()=='/'?value:root+"/"+value;};auto state=read_file(path("state"));auto prepared=checked(prepare_catalog(read_file(path("catalog")),state));WorldBundle world(prepared,checked(decode_world_seed(state,*prepared)));require(world.validate());auto before=checked(export_world_state(world));auto checkpoint=checked(encode_world_checkpoint(world));auto player=world.common.state().metadata.player_actor_id;auto& expected=*row.find("characters");allowed(world.common.state(),world.interaction,*prepared->interaction,*prepared->common,player,*expected.find(player));++selected;
        if(auto encounter=pending_encounter(world.interaction,player)){auto peer=encounter->leaders[0]==player?encounter->leaders[1]:encounter->leaders[0];allowed(world.common.state(),world.interaction,*prepared->interaction,*prepared->common,peer,*expected.find(peer));++selected;}
        denied(world.common.state(),world.interaction,*prepared->interaction,*prepared->common,"actor.blackwater.commander");denied(world.common.state(),world.interaction,*prepared->interaction,*prepared->common,"not.a.visible.person");denied(world.common.state(),world.interaction,*prepared->interaction,*prepared->common,"");
        if(!world.interaction.interactions.empty()&&world.interaction.interactions.front().phase=="choice")selection_edges(world,expected);
        exact(before,checked(export_world_state(world)));assert(checkpoint==checked(encode_world_checkpoint(world)));++cases;
    }assert(cases==6&&selected==10);std::cout<<"background_journey_character: PASS (6 actual GD sources; 10 selected self/peer portraits; real A public fields; denied reads never call A; party early return, speaker/observer isolation, deep copies and unchanged world)\n";}
