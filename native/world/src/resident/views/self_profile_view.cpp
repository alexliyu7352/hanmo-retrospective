// 原共同人物、成长等级及真实角色的有限只读投影；不复制成长结算或创建模板数值。
#include "self_profile_view.hpp"
#include "character_presentation.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/storage/common_codec.hpp"
#include <array>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using A=V::Array;
// 统一拒绝，不把缺失或无权观察的人物资料附在错误中。
Outcome<V> invalid(const StableId& code="InvalidSelfProfileSource") {
    return {false,{},common_failure(code).error};
}
// 世界身份使用既有边界的32位小写十六进制格式，不创造新身份或调用随机源。
StableId world_id_text(const WorldId& id) {
    static constexpr char digits[]="0123456789abcdef";StableId result;result.reserve(32);
    for(const auto byte:id){result+=digits[byte>>4];result+=digits[byte&15];}return result;
}
// ID仅限原有限页面协议长度；合法空组织表示这个角色没有组织归属。
bool bounded_id(const StableId& id,bool empty=false) { return (empty||!id.empty())&&id.size()<=256; }
}
Outcome<BoundaryValue> self_profile_view(const WorldBundle& world,const StableId& observer) {
    const auto& state=world.common.state();
    if(observer!=state.metadata.player_actor_id||observer.empty())return invalid("SelfProfileNotCurrentPlayer");
    const auto* actor=state.actors.find(observer);
    if(!actor||!world.catalog||!world.catalog->common||!bounded_id(observer))return invalid();
    CommonCharacterPresentation presentation(state,*world.catalog->common);const auto character=presentation.character(observer);
    if(!character.ok)return {false,{},character.error};
    if(character.value.actor_view.display_name.size()>512)return invalid();
    A attributes,skills,roles;
    const std::array<std::pair<const char*,Integer>,5> values{{
        {"leadership",actor->attributes.leadership},{"martial",actor->attributes.martial},
        {"intellect",actor->attributes.intellect},{"administration",actor->attributes.administration},
        {"charisma",actor->attributes.charisma}}};
    for(const auto& value:values){
        if(value.second<1||value.second>110)return invalid();
        attributes.push_back(boundary::obj({{"attribute_id",V{value.first}},{"value",V{value.second}}}));
    }
    if(world.catalog->growth&&world.catalog->growth->cultivation){
        for(const auto* id:{"infantry","cavalry","archery","martial","tactics","debate","negotiation","rites","commerce","transport"}){
            const auto xp=growth_skill_xp(actor->skills,id),level=growth_skill_level(xp);if(level<0)return invalid();
            skills.push_back(boundary::obj({{"skill_id",V{id}},{"xp",V{xp}},{"level",V{level}}}));
        }
    }
    // 只认真实当前角色持有人；组织成员身份不等于任职，不扩散其它人物或组织私账。
    for(const auto& row:state.roles.rows()){
        if(!row.second.enabled||row.second.actor_id!=observer)continue;
        const auto* role=world.catalog->common->roles.find(row.first);
        if(!role||roles.size()>=64||!bounded_id(row.first)||!bounded_id(role->place_id)||
            (role->organization_id&&!bounded_id(*role->organization_id)))return invalid();
        roles.push_back(boundary::obj({{"role_id",V{row.first}},{"place_id",V{role->place_id}},
            {"organization_id",V{role->organization_id.value_or("")}}}));
    }
    StableId action_kind;
    if(!actor->action_id.empty()){
        const auto* action=state.actions.find(actor->action_id);
        if(!action||std::find(action->participants.begin(),action->participants.end(),observer)==action->participants.end()||
            !bounded_id(action->kind))return invalid();
        action_kind=action->kind;
    }
    const auto& identity=state.identity;
    return {true,boundary::obj({{"kind",V{"self_profile"}},{"actor_id",V{observer}},
        {"display_name",V{character.value.actor_view.display_name}},{"age_years",V{character.value.actor_view.age_years}},
        {"attributes",V{std::move(attributes)}},{"skills",V{std::move(skills)}},{"roles",V{std::move(roles)}},
        {"status",boundary::obj({{"alive",V{actor->alive}},{"available",V{actor->available}},
            {"hp",V{actor->hp}},{"fatigue",V{actor->fatigue}},{"injury",V{actor->injury}},{"action_kind",V{action_kind}}})},
        {"source",boundary::obj({{"world_id",V{world_id_text(identity.world_id)}},
            {"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}}})}}),{}};
}
}
