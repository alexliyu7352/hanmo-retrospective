// 对方当面说出的接受或拒绝只在本人当前会话显示；不枚举全部NPC作者意愿。
#include "party_recruitment_consent.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/views/common_presentation.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {namespace {
using V=BoundaryValue;
// 页和报价绑定原世界、恢复epoch、提交与半日，避免旧回答成为新节点授权。
V identity(const WorldIdentity& value){StableId id;const char* hex="0123456789abcdef";for(auto byte:value.world_id){id+=hex[byte>>4];id+=hex[byte&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{value.execution_epoch}},{"revision",V{value.revision}},{"tick",V{value.tick}}});}
}
Outcome<BoundaryValue> query_party_recruitment_consent(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const PartyRecruitmentConsentCatalog* policy,const InteractionAccess& access){
    if(!policy)return {false,{},common_failure("PartyRecruitmentProfileRequired",{},ErrorKind::unsupported_capability).error};const auto& actor=common.state().metadata.player_actor_id;if(!common.actor(actor)||!catalog.common)return {false,{},common_failure("UnknownPartyRecruitmentObserver").error};V::Array rows;auto page=[&]()->Outcome<V>{return {true,boundary::obj({{"ok",V{true}},{"source_identity",identity(common.state().identity)},{"observer_actor_id",V{actor}},{"rows",V{std::move(rows)}}}),{}};};
    const InteractionSession* current=nullptr;for(const auto& row:state.interactions)if(row.actor_id==actor){if(current)return {false,{},common_failure("AmbiguousAuthoredNodeSource").error};current=&row;}if(!current||current->phase!="say")return page();
    bool shown=false;for(const auto& row:policy->declarations)if(row.leader_actor_id==actor&&row.companion_actor_id==current->context.provider_actor_id&&row.role_id==current->context.role_id&&row.dialogue_id==current->dialogue_id&&row.node_id==current->node_id)shown=true;if(!shown)return page();
    auto quote=preview_party_recruitment_consent(common,state,catalog,policy,access,actor,current->context);if(!quote.ok)return {false,{},quote.error};if(!quote.value)return {false,{},common_failure("MissingPartyRecruitmentDeclaration").error};const auto& q=*quote.value;CommonPresentation names(common.state(),*catalog.common);auto name=names.display_name(q.source.speaker_actor_id);if(!name.ok)return {false,{},name.error};V::Array actions;bool joined_here=false;if(const auto* person=common.actor(q.source.speaker_actor_id))if(const auto* following=std::get_if<FollowingLocation>(&person->location)){const auto* party=common.state().parties.find(following->party_id);joined_here=party&&party->leader_id==actor&&std::find(party->members.begin(),party->members.end(),person->id)!=party->members.end();}
    if(q.decision=="accept"&&!joined_here){
        const JoinParty command{q.source.speaker_actor_id,q.source.meeting.context,{}};auto ready=preview_interaction(common,state,catalog,access,actor,command);const auto params=boundary::obj({{"actor_id",V{command.actor_id}},{"context",interaction_codec::encode_context(*command.context)}});
        actions.push_back(boundary::obj({{"command",V{StableId{"join_party"}}},{"params",params},{"text",V{StableId{"确认邀请对方加入同行"}}},{"available",V{ready.ok}},{"code",V{ready.ok?StableId{}:ready.error.original.code}}}));
    }
    rows.push_back(boundary::obj({{"declaration_id",V{q.declaration_id}},{"companion_actor_id",V{q.source.speaker_actor_id}},{"display_name",V{name.value}},{"decision",V{q.decision}},{"joined_here",V{joined_here}},{"text",V{joined_here?StableId{"对方已经在你的队伍中；成员与行旅费用仍以原同行约定为准。"}:q.decision=="accept"?StableId{"对方当面表示愿意同行；是否邀请加入，由你确认。"}:StableId{"对方当面谢绝了这次同行；这份声明不会建立成员关系。"}}},{"cost_per_tick",V{q.cost_per_tick}},{"terms_text",V{"同行行旅每半日附加 "+std::to_string(q.cost_per_tick)+" 钱，由领队照原行旅规则支付；加入时不扣款，停驻不收此路费。"}},{"actions",V{std::move(actions)}}}));return page();
}
}
