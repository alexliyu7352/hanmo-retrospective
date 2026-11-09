// 动态本人开口是实际会话入口；亲缘只限定可用模板，绝不把亲缘或玩家选择等同于接受。
#include "born_personal_dialogue_detail.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
StableId place(const InteractionAccess& a,const StableId& id){if(!a.resolve_location)return {};auto where=a.resolve_location(id);const auto* at=std::get_if<SettlementLocation>(&where);return at?at->place_id:StableId{};}
}
const BornPersonalDialogueTemplate* born_personal_dialogue_template(const BornPersonalDialogueCatalog& c,const StableId& id){for(const auto& t:c.templates)if(t.id==id)return &t;return nullptr;}
Outcome<BornPersonalDialogueQuote> preview_born_personal_dialogue(const CommonStore& common,const InteractionState& interactions,const BornPersonalDialogueState& state,const InteractionCatalog& definitions,const BornPersonalDialogueCatalog& catalog,const InteractionAccess& access,const BornPersonalDialogueSources& sources,const StableId& actor,const OpenBornPersonalDialogue& request){
    auto valid=validate_born_personal_dialogue_catalog(catalog,definitions);if(!valid.ok)return {false,{},valid.error};
    if(state.version!=1||actor!=common.state().metadata.player_actor_id)return fail<BornPersonalDialogueQuote>("BornPersonalDialoguePlayerRequired");
    const auto* t=born_personal_dialogue_template(catalog,request.template_id);const auto* listener=common.actor(actor);const auto* provider=common.actor(request.provider_actor_id);
    if(!t||!listener||!provider||actor==provider->id)return fail<BornPersonalDialogueQuote>("BornPersonalDialogueNotAvailable");
    if(!access.resolve_location||!access.battle_occupied||!sources.born)return fail<BornPersonalDialogueQuote>("BornPersonalDialogueSourceRequired");
    const auto here=place(access,actor);if(here.empty()||here!=place(access,provider->id)||!definitions.common||!definitions.common->places.find(here)||!listener->alive||!listener->available||!provider->alive||!provider->available)return fail<BornPersonalDialogueQuote>("BornPersonalDialogueNotAvailable");
    // 先核现场才读出生资料；远方任意actor参数不得成为出生/亲缘查询口。
    if(!listener->action_id.empty()||!provider->action_id.empty()||access.battle_occupied(actor)||access.battle_occupied(provider->id)||born_personal_detail::occupied(interactions,actor)||born_personal_detail::occupied(interactions,provider->id)||pending_encounter(interactions,actor)||pending_encounter(interactions,provider->id))return fail<BornPersonalDialogueQuote>("BornPersonalDialogueBusy");
    auto born=sources.born(provider->id);if(!born.ok)return {false,{},born.error};if(!born.value||!born.value->adult||!born.value->alive||born.value->prototype_id!=t->prototype_id||born.value->actor_id!=provider->id||!born.value->before_fact_id.empty()||born.value->observed_tick!=common.tick()||!born_personal_detail::identity(born.value->source_identity,common.state().identity)||!born_personal_detail::is_parent(*born.value,actor))return fail<BornPersonalDialogueQuote>("BornPersonalDialogueNotAvailable");
    if(state.openings.size()>=65536)return fail<BornPersonalDialogueQuote>("BornPersonalDialogueHistoryLimit");
    BornPersonalDialogueQuote q;q.source_identity=common.state().identity;q.listener_actor_id=actor;q.template_id=t->id;q.dialogue_id=t->dialogue_id;q.context.place_id=here;q.context.provider_actor_id=provider->id;q.context.kind="narrative";q.born=*born.value;return {true,std::move(q),{}};
}
Outcome<StableId> open_born_personal_dialogue_in_candidate(CommonStore& common,InteractionState& interactions,BornPersonalDialogueState& state,const InteractionCatalog& definitions,const BornPersonalDialogueCatalog& catalog,const InteractionAccess& access,const BornPersonalDialogueSources& sources,const StableId& actor,const OpenBornPersonalDialogue& request,const BornPersonalDialogueQuote& quote){
    if(!born_personal_detail::identity(quote.source_identity,common.state().identity)||quote.listener_actor_id!=actor)return fail<StableId>("StaleBornPersonalDialogueQuote");
    auto fresh=preview_born_personal_dialogue(common,interactions,state,definitions,catalog,access,sources,actor,request);if(!fresh.ok)return {false,{},fresh.error};
    if(fresh.value.template_id!=quote.template_id||fresh.value.dialogue_id!=quote.dialogue_id||!born_personal_detail::context(fresh.value.context,quote.context)||!born_personal_detail::born_identity(fresh.value.born,quote.born))return fail<StableId>("StaleBornPersonalDialogueQuote");
    const auto* d=born_personal_detail::dialogue(definitions,quote.dialogue_id);if(!d)return fail<StableId>("InvalidBornPersonalDialogue");
    auto session=common.next_id("interaction");if(!session.ok)return session;auto fact=common.next_id(born_personal_dialogue_opened_kind);if(!fact.ok)return fact;
    BornPersonalDialogueOpening row;row.fact_id=fact.value;row.template_id=quote.template_id;row.session_id=session.value;row.listener_actor_id=actor;row.provider_actor_id=quote.context.provider_actor_id;row.dialogue_id=quote.dialogue_id;row.place_id=quote.context.place_id;row.birth_fact_id=quote.born.birth_fact_id;row.creation_fact_id=quote.born.creation_fact_id;row.prototype_id=quote.born.prototype_id;row.tick=common.tick();
    auto valid=common.append_fact({fact.value,born_personal_dialogue_opened_kind,born_personal_dialogue_module});if(!valid.ok)return {false,{},valid.error};if(!state.openings.insert(fact.value,row))return fail<StableId>("DuplicateBornPersonalDialogueOpening");
    auto occupancy_before=capture_actor_social_occupancy(common,interactions,access,actor);if(!occupancy_before.ok)return {false,{},occupancy_before.error};
 InteractionSession current;current.actor_id=actor;current.id=session.value;current.dialogue_id=quote.dialogue_id;current.node_id=d->entry;current.phase="stepping";current.context=quote.context;interactions.interactions.push_back(std::move(current));valid=open_interaction_response_boundary_in_candidate(interactions,definitions,session.value);if(!valid.ok)return {false,{},valid.error};valid=record_captured_actor_social_occupancy(access,actor,occupancy_before.value,ActorSocialOccupancyCause::born_dialogue_open,session.value,actor,SourceRef{born_personal_dialogue_module,born_personal_dialogue_opened_kind,fact.value});if(!valid.ok)return {false,{},valid.error};
    // 完整外层保存点包括新独立段；不可因pump失败只删除会话而留下可伪造的打开凭据。
    valid=pump_dialogue(common,interactions,definitions,access,actor);if(!valid.ok)return {false,{},valid.error};return fact;
}
}
