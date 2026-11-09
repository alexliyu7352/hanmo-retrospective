// 生命终点页只展示当前控制人物的真实来源及尚待办理的权利，不代办继任或处分遗产。
#include "family_death.hpp"
namespace hanmo::resident {
// 页面固定一个人物并绑定完整会话身份；调用方先核同代Military/Naval历史。
Outcome<BoundaryValue> query_family_death_page(const CommonStore& common,const FamilyDeathState& state,const FamilyDeathOriginRead& read){
    using namespace boundary;using V=BoundaryValue;const auto& actor_id=common.state().metadata.player_actor_id;const auto* actor=common.actor(actor_id);if(!actor)return {false,{},common_failure("UnknownActor",actor_id).error};
    auto valid=validate_family_deaths(common,state,read,true);if(!valid.ok)return {false,{},valid.error};const auto& identity=common.state().identity;StableId world;const char* digits="0123456789abcdef";for(auto byte:identity.world_id){world+=digits[byte>>4];world+=digits[byte&15];}
    V cause=obj({});StableId status=actor->alive?"living":"death_source_not_recorded";
    for(const auto& row:state.deaths.rows())if(row.second.actor_id==actor_id){const auto& record=row.second;status="awaiting_legacy_review";cause=obj({{"death_fact_id",V{row.first}},{"cause_kind",V{record.cause_kind}},{"cause_fact_id",V{record.cause_fact_id}},{"battle_id",V{record.battle_id}},{"death_tick",V{record.death_tick}},{"recorded_tick",V{record.recorded_tick}}});break;}
    return {true,obj({{"actor_id",V{actor_id}},{"source_identity",obj({{"world_id",V{world}},{"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}},{"tick",V{identity.tick}}})},{"status",V{status}},{"death",std::move(cause)},{"control_confirmation_required",V{!actor->alive}},{"assets_transferred",V{false}},{"public_authority_transferred",V{false}}}),{}};
}
}
