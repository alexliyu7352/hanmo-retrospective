// 完整来源在原出生/创建/亲缘/死因/控制史之后验证；后来死亡和换控制者不得倒改合法旧会面。
#include "born_personal_dialogue_detail.hpp"
#include <set>
namespace hanmo::resident {
Outcome<bool> match_born_personal_dialogue_session(const CommonStore& common,const BornPersonalDialogueState& state,const BornPersonalDialogueCatalog& catalog,const InteractionCatalog& definitions,const InteractionSession& session){
    const auto* o=born_personal_detail::opening(state,session.id);if(!o)return {true,false,{}};
    auto fail=[&]()->Outcome<bool>{return {false,{},common_failure("BornPersonalDialogueSessionMismatch",session.id).error};};
    const auto* t=born_personal_dialogue_template(catalog,o->template_id);const auto* d=born_personal_detail::dialogue(definitions,o->dialogue_id);const auto* head=common.state().facts.find(o->fact_id);const auto sid=born_personal_detail::serial(common,o->session_id,"interaction"),fid=born_personal_detail::serial(common,o->fact_id,born_personal_dialogue_opened_kind);
    if(state.version!=1||!t||!d||!d->context_role.empty()||t->dialogue_id!=o->dialogue_id||t->prototype_id!=o->prototype_id||!head||head->id!=o->fact_id||head->domain!=born_personal_dialogue_module||head->kind!=born_personal_dialogue_opened_kind||!sid||!fid||*sid+1!=*fid||session.actor_id!=o->listener_actor_id||session.dialogue_id!=o->dialogue_id||session.context.provider_actor_id!=o->provider_actor_id||session.context.place_id!=o->place_id||!born_personal_detail::narrative(session.context)||session.regional_binding||session.regional_action_committed||session.pending_command||!session.pending_action_id.empty()||session.trade_completed||session.last_outcome.present)return fail();
    const auto* node=born_personal_detail::node(*d,session.node_id);if(!node)return fail();
    // 原begin/Next/Choose先写stepping，再由pump校验本绑定并落say/choice或删除end。
    // 这是同候选过渡识别，不是可冷存继续点；下方完整域门与原Interaction门均拒stepping。
    if(session.phase=="stepping"){if(node->kind!="say"&&node->kind!="choice"&&node->kind!="end")return fail();}
    else if((session.phase!="say"&&session.phase!="choice")||node->kind!=session.phase)return fail();return {true,true,{}};
}
Status validate_born_personal_dialogues(const CommonStore& common,const InteractionState& interactions,const BornPersonalDialogueState& state,const InteractionCatalog& definitions,const BornPersonalDialogueCatalog& catalog,const BornPersonalDialogueSources& sources){
    auto valid=validate_born_personal_dialogue_catalog(catalog,definitions);if(!valid.ok)return valid;if(state.version!=1||state.openings.size()>65536)return common_failure("InvalidBornPersonalDialogueState");
    if(!sources.born_before||!sources.presence_before||!sources.player_before)return common_failure("BornPersonalDialogueHistorySourceRequired");
    std::set<StableId> sessions;std::vector<StableId> headers;Integer last=0,last_tick=0;
    for(const auto& row:common.state().facts.rows())if(row.second.domain==born_personal_dialogue_module||row.second.kind==born_personal_dialogue_opened_kind){if(row.second.id!=row.first||row.second.domain!=born_personal_dialogue_module||row.second.kind!=born_personal_dialogue_opened_kind||!state.openings.find(row.first))return common_failure("InvalidBornPersonalDialogueHeader",row.first);headers.push_back(row.first);}
    if(headers!=state.openings.order())return common_failure("BornPersonalDialogueFactOrderMismatch");
    for(const auto& row:state.openings.rows()){
        const auto& o=row.second;const auto id=born_personal_detail::serial(common,row.first,born_personal_dialogue_opened_kind),sid=born_personal_detail::serial(common,o.session_id,"interaction");const auto* t=born_personal_dialogue_template(catalog,o.template_id);
        if(o.fact_id!=row.first||!id||!sid||*sid+1!=*id||*sid<=last||!sessions.insert(o.session_id).second||!t||t->prototype_id!=o.prototype_id||t->dialogue_id!=o.dialogue_id||o.listener_actor_id.empty()||o.listener_actor_id==o.provider_actor_id||!definitions.common->places.find(o.place_id)||o.tick<last_tick||o.tick>common.tick())return common_failure("InvalidBornPersonalDialogueOpening",row.first);last=*id;last_tick=o.tick;
        auto born=sources.born_before(o.provider_actor_id,o.tick,o.fact_id);if(!born.ok)return {false,{},born.error};if(!born.value||!born_personal_detail::opening_born(o,*born.value)||born.value->before_fact_id!=o.fact_id||born.value->observed_tick!=o.tick||born.value->source_identity.world_id!=common.state().identity.world_id)return common_failure("BornPersonalDialogueBirthMismatch",row.first);
        auto present=sources.presence_before(o.listener_actor_id,o.tick,o.fact_id);if(!present.ok)return {false,{},present.error};if(present.value.actor_id!=o.listener_actor_id||present.value.before_fact_id!=o.fact_id||present.value.observed_tick!=o.tick||present.value.source_identity.world_id!=common.state().identity.world_id||!present.value.alive)return common_failure("BornPersonalDialogueListenerAbsent",row.first);
        auto player=sources.player_before(o.tick,o.fact_id);if(!player.ok)return {false,{},player.error};if(player.value!=o.listener_actor_id)return common_failure("BornPersonalDialogueHistoricalPlayerRequired",row.first);
    }
    std::set<StableId> current;for(const auto& s:interactions.interactions){auto match=match_born_personal_dialogue_session(common,state,catalog,definitions,s);if(!match.ok)return {false,{},match.error};if(match.value){for(const auto& other:interactions.interactions)if(&other!=&s&&(born_personal_detail::involved(other,s.actor_id)||born_personal_detail::involved(other,s.context.provider_actor_id)))return common_failure("BornPersonalDialogueDoubleOccupation",s.id);if(s.phase!="say"&&s.phase!="choice")return common_failure("InvalidBornPersonalDialogueContinuePoint",s.id);if(!current.insert(s.id).second)return common_failure("DuplicateBornPersonalDialogueSession",s.id);}}
    return common_success();
}
}
