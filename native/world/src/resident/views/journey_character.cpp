// 复用原sidebar选择顺序与当前会话说话者；公开人物字段/外貌编码完全交A的同候选端口。
#include "journey_character.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 未选择与无效人物用同一原错误，既不查私有记录，也不暴露目标是否实际存在。
Outcome<CharacterPresentationView> unavailable(){return {false,{},common_failure("InvalidPresentationActor").error};}
// 当前对白唯一节点只供speaker替换；不读取选项文本、条件或其它人物会话。
const DialogueNode* selected_node(const InteractionCatalog& catalog,const InteractionSession& session){for(auto& dialogue:catalog.dialogues)if(dialogue.id==session.dialogue_id){for(auto& node:dialogue.nodes)if(node.id==session.node_id)return &node;break;}return nullptr;}
}
Outcome<CharacterPresentationView> query_selected_character(const CommonState& common,const InteractionState& interaction,const InteractionCatalog& catalog,const CharacterPresentationRead& presentation,const StableId& requested){
    const auto& observer=common.metadata.player_actor_id;if(!common.actors.find(observer))return unavailable();
    if(requested==observer)return presentation.character(requested);
    const auto* encounter=pending_encounter(interaction,observer);
    if(encounter){const auto& peer=encounter->leaders[0]==observer?encounter->leaders[1]:encounter->leaders[0];if(requested==peer)return presentation.character(requested);}
    else {
        // 原Location.party_id取第一支真实含本人的party；不能用army优先的members接口代替。
        for(auto& row:common.parties.rows()){const auto& party=row.second;if(std::find(party.members.begin(),party.members.end(),observer)==party.members.end())continue;if(requested!=party.leader_id&&std::find(party.members.begin(),party.members.end(),requested)!=party.members.end())return presentation.character(requested);break;}
    }
    for(auto& session:interaction.interactions){if(session.actor_id!=observer)continue;auto node=selected_node(catalog,session);if(!node)return {false,{},common_failure("InvalidDialogueContinuePoint").error};
        if(session.phase!="say"&&session.phase!="choice"&&session.phase!="action"&&session.phase!="trade"&&session.phase!="waiting")return {false,{},common_failure("InvalidDialogueContinuePoint").error};
        auto speaker=session.context.provider_actor_id;if(session.phase=="say")speaker=node->speaker=="$provider"?session.context.provider_actor_id:(node->speaker=="$player"?observer:node->speaker);
        if(requested==speaker)return presentation.character(requested);break;
    }
    return unavailable();
}
} // 整个过滤只借const typed状态，拒绝时不会读取/生成外貌，成功值不含状态引用。
