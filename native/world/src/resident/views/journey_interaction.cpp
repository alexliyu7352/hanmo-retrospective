// 原有限行旅/对白读取：身份文本经PresentationRead，选项条件复用规则；不预览或执行任何动作。
#include "journey_interaction.hpp"
#include "resident/model/common_model.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/siege_world.hpp"
namespace hanmo::resident {
namespace {
constexpr std::size_t text_limit=4096;
constexpr Integer option_page_limit=64;
// 查询错误保留领域来源；这里的有限显示约束不修改作者文本或世界事实。
template<class T> Outcome<T> rejected(const StableId& code,const StableId& path={},ErrorKind kind=ErrorKind::rule){auto error=common_failure(code,path,kind).error;error.phase="query";error.domain="interaction";return {false,{},std::move(error)};}
bool bounded(const std::string& value){return value.size()<=text_limit;}
const InteractionSession* session_for(const InteractionState& s,const StableId& actor){for(auto& value:s.interactions)if(value.actor_id==actor)return &value;return nullptr;}
// 只读取已选会话的作者节点，不扫描或泄漏其它人物的对白。
const DialogueNode* session_node(const InteractionCatalog& c,const InteractionSession& session){for(auto& dialogue:c.dialogues)if(dialogue.id==session.dialogue_id){for(auto& node:dialogue.nodes)if(node.id==session.node_id)return &node;break;}return nullptr;}
StableId speaker_for(const DialogueNode& node,const InteractionSession& session){if(session.phase!="say")return session.context.provider_actor_id;return node.speaker=="$provider"?session.context.provider_actor_id:(node.speaker=="$player"?session.actor_id:node.speaker);}
bool bounded_context(const InteractionContext& c){return bounded(c.place_id)&&bounded(c.provider_actor_id)&&bounded(c.role_id)&&(!c.kind||bounded(*c.kind))&&(!c.encounter_id||bounded(*c.encounter_id));}
bool bounded_command(const DialogueCommand& c){return bounded(c.kind)&&bounded(c.offer_id)&&bounded(c.shop_id)&&bounded(c.service_id)&&bounded(c.target_actor_id)&&bounded(c.activity)&&bounded(c.skill_id)&&bounded(c.place_id)&&(!c.context||bounded_context(*c.context));}
}
Outcome<JourneyEncounterView> query_journey_encounter(const CommonRead& common,const InteractionState& state,const InteractionCatalog& catalog,const PresentationRead& presentation,const StableId& observer){
    auto actor=common.actor(observer);if(!actor)return rejected<JourneyEncounterView>("UnknownActor");JourneyEncounterView out;
    if(!actor->action_id.empty()){auto action=common.action(actor->action_id);if(!action)return rejected<JourneyEncounterView>("InvalidActionContext");out.travel_active=action->kind=="travel";}
    auto encounter=pending_encounter(state,observer);if(!encounter)return {true,std::move(out),{}};
    const auto& provider=encounter->leaders[0]==observer?encounter->leaders[1]:encounter->leaders[0];auto name=presentation.display_name(provider);if(!name.ok)return {false,{},name.error};if(!bounded(encounter->id)||!bounded(provider)||!bounded(name.value))return rejected<JourneyEncounterView>("JourneyViewTextLimit");out.encounter=JourneyEncounterPeer{encounter->id,provider,std::move(name.value)};
    if(auto session=session_for(state,observer)){auto node=session_node(catalog,*session);if(!node)return rejected<JourneyEncounterView>("InvalidDialogueContinuePoint");auto speaker=speaker_for(*node,*session);auto display=presentation.display_name(speaker);if(!display.ok)return {false,{},display.error};if(!bounded(speaker)||!bounded(display.value))return rejected<JourneyEncounterView>("JourneyViewTextLimit");out.dialogue_speaker=JourneySpeakerView{std::move(speaker),std::move(display.value)};}
    return {true,std::move(out),{}};
}
// 沿原ActionContext.needs_input的CombatSession→SiegeWorld→Encounter短路；历史或他人会话不阻断本人页面。
Outcome<bool> query_journey_input_need(const CommonRead& common,const PersonalState& personal,const MilitaryState& military,const InteractionState& interaction,const StableId& observer){
    if(!common.actor(observer))return rejected<bool>("UnknownActor");
    if(personal_occupied(personal,observer)||military_actor_in_battle(military,observer))return {true,true,{}};
    if(!siege_for_actor(military,observer).empty())return {true,true,{}};
    return {true,pending_encounter(interaction,observer)!=nullptr,{}};
}
Outcome<std::optional<JourneyLogisticsView>> query_own_army_logistics(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& catalog,const StableId& observer){
    if(!common.actor(observer))return rejected<std::optional<JourneyLogisticsView>>("UnknownActor");auto army=active_army_for_actor(military,observer);if(!army||army->commander_actor_id!=observer)return {true,std::nullopt,{}};
    auto quote=preview_army_supply(common,military,catalog,army->id);if(!quote.ok){if(quote.error.kind==ErrorKind::rule&&(quote.error.original.code=="UnknownArmy"||quote.error.original.code=="ArmyNotActive"))return {true,std::nullopt,{}};return {false,{},quote.error};}auto& q=quote.value;return {true,JourneyLogisticsView{q.available,q.required,q.mouth_count,q.full_ticks,q.shortage},{}};
}
Outcome<JourneyDialogueView> query_interaction_view(const CommonRead& common,const InteractionState& state,const InteractionCatalog& catalog,const InteractionAccess& access,const PresentationRead& presentation,const StableId& observer,Integer cursor){
    if(cursor<0)return rejected<JourneyDialogueView>("InvalidJourneyCursor");if(!common.actor(observer))return rejected<JourneyDialogueView>("UnknownActor");JourneyDialogueView out;auto session=session_for(state,observer);if(!session){if(cursor)return rejected<JourneyDialogueView>("InvalidJourneyCursor");return {true,std::move(out),{}};}
    auto node=session_node(catalog,*session);if(!node)return rejected<JourneyDialogueView>("InvalidDialogueContinuePoint");if(!bounded(session->phase)||!bounded(session->node_id)||!bounded_context(session->context)||!bounded(session->last_outcome.code))return rejected<JourneyDialogueView>("JourneyViewTextLimit");out.phase=session->phase;out.context=session->context;out.node_id=session->node_id;out.last_outcome=session->last_outcome;
    if(out.phase=="say"||out.phase=="choice"){auto text=presentation.text(node->text_key);if(!text.ok)return {false,{},text.error};if(!bounded(text.value))return rejected<JourneyDialogueView>("JourneyViewTextLimit");out.text=std::move(text.value);}
    if(out.phase=="say"){auto speaker=speaker_for(*node,*session);if(!bounded(speaker))return rejected<JourneyDialogueView>("JourneyViewTextLimit");out.speaker=std::move(speaker);}
    else if(out.phase=="choice"){
        out.has_options=true;out.cursor=cursor;
        for(auto& option:node->options){auto visible=all_conditions(common,state,access,observer,option.conditions);if(!visible.ok)return {false,{},visible.error};if(!visible.value)continue;auto index=out.total_rows++;if(index<cursor||index-cursor>=option_page_limit)continue;auto text=presentation.text(option.text_key);if(!text.ok)return {false,{},text.error};if(!bounded(option.id)||!bounded(text.value))return rejected<JourneyDialogueView>("JourneyViewTextLimit");out.options.push_back({option.id,std::move(text.value)});}
        if(cursor>out.total_rows)return rejected<JourneyDialogueView>("InvalidJourneyCursor");if(cursor+static_cast<Integer>(out.options.size())<out.total_rows)out.next_cursor=cursor+static_cast<Integer>(out.options.size());
    }else if(out.phase=="action"||out.phase=="trade"||out.phase=="waiting"){
        if(!session->pending_command)return rejected<JourneyDialogueView>("InvalidDialogueContinuePoint");if(!bounded_command(*session->pending_command)||!bounded(session->pending_action_id))return rejected<JourneyDialogueView>("JourneyViewTextLimit");out.command=session->pending_command;out.pending_action_id=session->pending_action_id;
    }else if(out.phase!="say")return rejected<JourneyDialogueView>("InvalidDialogueContinuePoint");
    if(!out.has_options&&cursor)return rejected<JourneyDialogueView>("InvalidJourneyCursor");return {true,std::move(out),{}};
}
} // 返回值不持CommonState/领域/目录引用，UI修改副本不会改变下一次查询或提交。
