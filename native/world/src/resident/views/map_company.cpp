#include "resident/domains/military/military_model.hpp"
// 本人军旅位置上下文复用原活动军团规则，不枚举远方标记或推算军令准入。
#include "map_company.hpp"
namespace hanmo::resident {
namespace {
// 有军团记录但没有一致本人来源时明确拒绝，不能降成普通旅人掩盖坏引用。
Outcome<MapCompanyPage> failure(const StableId& code){return {false,{},common_failure(code,"map_company").error};}
}
Outcome<MapCompanyPage> query_map_company(const CommonStore& common,const MilitaryState& military,const PresentationRead& names,const StableId& observer){
    if(observer!=common.state().metadata.player_actor_id)return failure("MapCompanyObserverRequired");
    const auto* person=common.actor(observer);if(!person||!person->alive)return failure("MapCompanyObserverUnavailable");
    if(observer.size()>4096)return failure("MapCompanyTextLimit");
    MapCompanyPage out;out.identity=common.state().identity;out.observer_actor_id=observer;
    const auto* army=active_army_for_actor(military,observer);const auto* attached=std::get_if<InArmyLocation>(&person->location);
    if(!army){if(attached)return failure("MapCompanySourceMissing");return {true,std::move(out),{}};}
    const bool commander=army->commander_actor_id==observer;
    if(army->id.empty()||army->commander_actor_id.empty()||army->id.size()>4096||army->commander_actor_id.size()>4096)return failure("MapCompanyTextLimit");
    if((commander&&(attached||std::holds_alternative<FollowingLocation>(person->location)))||(!commander&&(!attached||attached->army_id!=army->id)))return failure("MapCompanySourceMissing");
    const auto* leader=common.actor(army->commander_actor_id);if(!leader||!leader->alive)return failure("MapCompanySourceMissing");
    const auto name=names.display_name(army->commander_actor_id);if(!name.ok)return {false,{},name.error};
    if(name.value.size()>4096)return failure("MapCompanyTextLimit");
    out.army=MapArmyCompany{army->id,army->commander_actor_id,name.value,commander,common.tick()};return {true,std::move(out),{}};
}
BoundaryValue encode_map_company(const MapCompanyPage& page){
    using V=BoundaryValue;StableId hex;constexpr char digits[]="0123456789abcdef";
    for(const auto byte:page.identity.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}
    auto identity=boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{page.identity.execution_epoch}},{"revision",V{page.identity.revision}},{"tick",V{page.identity.tick}}});
    V army{V::Dict{}};if(page.army){const auto& value=*page.army;army=boundary::obj({{"army_id",V{value.army_id}},{"commander_actor_id",V{value.commander_actor_id}},{"commander_name",V{value.commander_name}},{"commanded_by_observer",V{value.commanded_by_observer}},{"observed_tick",V{value.observed_tick}}});}
    return boundary::obj({{"ok",V{true}},{"version",V{Integer(1)}},{"identity",std::move(identity)},{"observer_actor_id",V{page.observer_actor_id}},{"army",std::move(army)}});
}
}
