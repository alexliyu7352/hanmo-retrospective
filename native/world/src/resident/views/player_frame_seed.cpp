// 单次固定读组合仅搬运既有叶DTO；不复制规则、权限判断、文本选择或领域schema。
#include "player_frame_seed.hpp"
#include <algorithm>
#include <utility>

namespace hanmo::resident {
namespace {
using V=BoundaryValue;
using D=V::Dict;
// 提前结束只携带原叶错误或本组合边界错误；未完成的局部frame从不返回。
struct Stop { Outcome<V> result; };
// 组合错误具备明确query定位；不把预算拒绝伪装成领域成功或空页。
Outcome<V> failure(const char* code,const StableId& path={},ErrorKind kind=ErrorKind::internal){
    return {false,{},{{code,path},kind,"query","presentation","player_frame_seed",{}}};
}
// 组合只读取决定分支/选择人的少量字段；完整叶校验仍归原查询。
const V& field(const V& value,const char* key){const auto* p=value.find(key);if(!p)throw Stop{failure("InvalidPlayerFrameLeaf",key)};return *p;}
// 私有组装字段必须保持原字符串标签，不以隐式转换改变人选或阶段。
StableId string(const V& value,const char* path){const auto* p=value.as<StableId>();if(!p)throw Stop{failure("InvalidPlayerFrameLeaf",path)};return *p;}
// 回调返回的领域失败与必要读的负值原样停止；preview负值在另一入口保留。
V read(const PlayerFrameLeaf& leaf,const char* query,V args=V{D{}}){
    auto result=leaf(LiveOperationKind::query,V{D{{"query_id",V{StableId{query}}},{"args",std::move(args)}}});
    if(!result.ok)throw Stop{std::move(result)};
    if(!result.value.as<D>())throw Stop{failure("InvalidPlayerFrameLeaf",query)};
    if(const auto* ok=result.value.find("ok")){
        if(!ok->as<bool>())throw Stop{failure("InvalidPlayerFrameLeaf",query)};
        if(!*ok->as<bool>())throw Stop{std::move(result)};
    }
    return std::move(result.value);
}
// 计入typed节点、字典项、UTF8字符串及容器空间的保守占用；不渲染JSON、不扫描世界。
struct Budget {
    std::size_t left=player_frame_seed_byte_limit;
    void charge(std::size_t size){if(size>left)throw Stop{failure("PlayerFrameSeedLimit",{},ErrorKind::unsupported_capability)};left-=size;}
    void value(const V& item,std::size_t depth=0){
        if(depth>64)throw Stop{failure("PlayerFrameSeedLimit",{},ErrorKind::unsupported_capability)};
        charge(sizeof(V));
        if(auto text=item.as<StableId>())charge(text->size());
        else if(auto array=item.as<V::Array>())for(const auto& child:*array)value(child,depth+1);
        else if(auto object=item.as<D>())for(const auto& child:*object){charge(sizeof(D::value_type));charge(child.first.size());value(child.second,depth+1);}
    }
    void entry(const StableId& key,const V& item){charge(sizeof(D::value_type));charge(key.size());value(item);}
};
// 每个叶只计量一次后搬入拥有容器；2MiB上限在任何部分frame发布前生效。
void put(V& object,const char* key,V value,Budget& budget){budget.entry(key,value);object.as<D>()->emplace_back(key,std::move(value));}
// 页0严格保留旧cursor形状；不擅自拉取后续页或提高64行上限。
V page_zero(){return V{D{{"cursor",V{Integer{0}}}}};}
// 人物只来自服务器已选中的self/peer/speaker，去重保原显示顺序，最多三名。
void select(std::vector<StableId>& actors,const StableId& actor){if(!actor.empty()&&std::find(actors.begin(),actors.end(),actor)==actors.end())actors.push_back(actor);}
}
Outcome<BoundaryValue> query_player_frame_seed(const PlayerFrameLeaf& leaf,bool npc_society){
    if(!leaf)return failure("InvalidPlayerFrameLeaf","callback");
    try{
        Budget budget;V views{D{}},journey{D{}},characters{D{}};V::Array quotes;
        // 包装结构固定；叶预算另行逐项计量，空容器元数据只收一次。
        V root{D{{"ok",V{true}},{"views",V{D{}}},{"journey",V{D{}}},{"characters",V{D{}}},{"quotes",V{V::Array{}}}}};budget.value(root);
        for(const char* query:{"command_context","action_context","screen_context","hud_view","journey_input_need"})put(views,query,read(leaf,query),budget);
        put(views,"interaction_view",read(leaf,"interaction_view",page_zero()),budget);
        const auto player=string(field(field(views,"command_context"),"actor_id"),"command_context/actor_id");
        const auto& interaction=field(views,"interaction_view");const auto phase=string(field(interaction,"phase"),"interaction_view/phase");
        put(journey,"journey_location",read(leaf,"journey_location"),budget);
        put(journey,"journey_encounter",read(leaf,"journey_encounter"),budget);
        const auto& encounter=field(field(journey,"journey_encounter"),"encounter");
        if(!encounter.as<D>())return failure("InvalidPlayerFrameLeaf","journey_encounter/encounter");
        const bool has_encounter=!encounter.as<D>()->empty();StableId peer,encounter_id;
        if(has_encounter){peer=string(field(encounter,"provider_actor_id"),"encounter/provider_actor_id");encounter_id=string(field(encounter,"encounter_id"),"encounter/encounter_id");}
        else{
            put(journey,"journey_companions",read(leaf,"journey_companions",page_zero()),budget);
            put(journey,"journey_logistics",read(leaf,"journey_logistics"),budget);
            put(journey,"journey_travel",read(leaf,"journey_travel"),budget);
        }
        put(journey,"journey_map_position",read(leaf,"journey_map_position"),budget);
        put(journey,"journey_route",read(leaf,"journey_route",page_zero()),budget);
        put(journey,"journey_crossings",read(leaf,"journey_crossings",page_zero()),budget);
        std::vector<StableId> actors;select(actors,player);if(has_encounter)select(actors,peer);
        if(phase!="scene"){
            if(auto speaker=interaction.find("speaker"))select(actors,string(*speaker,"interaction_view/speaker"));
            else if(auto context=interaction.find("context")){if(auto provider=context->find("provider_actor_id"))select(actors,string(*provider,"interaction_view/context/provider_actor_id"));}
        }
        for(const auto& actor:actors){auto value=read(leaf,"character_input",V{D{{"actor_id",V{actor}}}});budget.entry(actor,value);characters.as<D>()->emplace_back(actor,std::move(value));}
        // 与原Main固定按钮相同的纯preview顺序；不以负quote停止整帧，更不submit。
        const auto quote=[&](const char* kind,V params=V{D{}}){
            auto result=leaf(LiveOperationKind::preview,V{D{{"actor_id",V{player}},{"kind",V{StableId{kind}}},{"params",params}}});
            if(!result.ok)throw Stop{std::move(result)};
            V row{D{{"kind",V{StableId{kind}}},{"actor_id",V{player}},{"params",std::move(params)},{"result",std::move(result.value)}}};budget.value(row);quotes.push_back(std::move(row));
        };
        for(const char* kind:{"advance","pause_travel","resume_travel","travel_wait","return","rescue","wait"})quote(kind);
        if(has_encounter)for(const char* kind:{"encounter_talk","encounter_fight"})quote(kind,V{D{{"encounter_id",V{encounter_id}}}});
        if(phase=="action")quote("dialogue_confirm");
        if((phase=="say"||phase=="choice")&&npc_society)quote("ask_news");
        if(phase!="scene"&&phase!="choice")quote("end_interaction");
        *root.find("views")=std::move(views);*root.find("journey")=std::move(journey);*root.find("characters")=std::move(characters);*root.find("quotes")=V{std::move(quotes)};
        return {true,std::move(root),{}};
    }catch(Stop& stopped){return std::move(stopped.result);}
}
} // 返回后同source整批独立拥有；世界替换不会修改已完成的frame seed。
