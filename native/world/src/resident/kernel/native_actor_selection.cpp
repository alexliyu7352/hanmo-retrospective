// 有界现存人物选择仅核冷作者输入；不访问运行世界、随机源或实例ID分配器。
#include "native_actor_selection.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {
// 只接显式旧双人或新名单政策；调用者不可把未知未来政策当成当前政策。
bool native_existing_actor_policy(const StableId& policy){return policy=="selected_existing_actor_v1"||policy=="selected_existing_actor_roster_v1";}
// 旧版恰两位，新版有界任意名单；所有候选必须有效，不能用一次合法选择遮住坏候选。
Status validate_native_actor_choices(const BoundaryValue& data,const BoundaryValue& scenario){try{
    using namespace boundary;
    const auto policy=get_string(field(scenario,"dynamic_world","scenario"),"founder_policy","dynamic_world");
    if(!native_existing_actor_policy(policy))unsupported("UnsupportedFounderPolicy","dynamic_world/founder_policy");
    const auto ids=strings(field(scenario,"playable_actor_ids","scenario"),"scenario/playable_actor_ids");
    const std::set<StableId> members(ids.begin(),ids.end());
    const bool valid_count=policy=="selected_existing_actor_v1"?ids.size()==2:!ids.empty()&&ids.size()<=4096;
    if(!valid_count||members.size()!=ids.size()||members.count(StableId{})||!members.count(get_string(scenario,"player_actor","scenario")))fail("InvalidPlayableActors","scenario/playable_actor_ids");
    const auto enabled=strings(field(field(scenario,"enabled","scenario"),"actors","enabled"),"enabled/actors");
    const auto active=strings(field(data,"active_static_actor_ids","data"),"data/active_static_actor_ids");
    const auto& initial=array(field(field(scenario,"initial","scenario"),"actors","initial"),"initial/actors");
    const auto& actors=field(data,"actors","data");object(actors,"data/actors");
    for(const auto& id:ids){
        if(std::count(enabled.begin(),enabled.end(),id)!=1||std::count(active.begin(),active.end(),id)!=1)fail("InvalidPlayableActors","scenario/playable_actor_ids/"+id);
        const auto* actor=actors.find(id);if(!actor)fail("InvalidPlayableActors","data/actors/"+id);
        const BoundaryValue* row=nullptr;
        for(const auto& candidate:initial)if(get_string(candidate,"actor_id","initial/actors")==id){if(row)fail("InvalidPlayableActors","initial/actors/"+id);row=&candidate;}
        if(!row)fail("InvalidPlayableActors","initial/actors/"+id);
        const auto age=get_int(*row,"age_years","initial/actors/"+id);
        if(age<18||age>120||get_int(*actor,"initial_age_years",id)!=age||!get_bool(*row,"alive",id)||!get_bool(*row,"available",id))fail("InvalidPlayableActors","initial/actors/"+id);
    }
    const auto& policies=array(field(scenario,"playable_actor_appearance_policies","scenario"),"scenario/playable_actor_appearance_policies");
    if(policies.size()!=ids.size())fail("InvalidPlayableActorAppearancePolicies","scenario/playable_actor_appearance_policies");
    std::set<StableId> covered;
    for(const auto& row:policies){
        fields(row,{"actor_id","policy"},{},"playable_actor_appearance_policies");
        const auto id=get_string(row,"actor_id","playable_actor_appearance_policies"),policy=get_string(row,"policy","playable_actor_appearance_policies");
        if(!members.count(id)||!covered.insert(id).second||(policy!="fixed_actor_appearance_composition_v1"&&policy!="preserve_existing_v1"))fail("InvalidPlayableActorAppearancePolicies","scenario/playable_actor_appearance_policies/"+id);
    }
    if(covered!=members)fail("InvalidPlayableActorAppearancePolicies","scenario/playable_actor_appearance_policies");
    return common_success();
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 先完整核作者政策再处理请求；缺失不得默选默认人物，越界ID不得归一化。
Outcome<StableId> select_native_actor(const BoundaryValue& data,const BoundaryValue& scenario,const std::optional<StableId>& requested){try{
    using namespace boundary;
    const auto valid=validate_native_actor_choices(data,scenario);if(!valid.ok)return {false,{},valid.error};
    if(!requested)fail("PlayableActorSelectionRequired","request/founder_selection");
    const auto ids=strings(field(scenario,"playable_actor_ids","scenario"),"scenario/playable_actor_ids");
    if(std::find(ids.begin(),ids.end(),*requested)==ids.end())fail("InvalidPlayableActor","request/founder_selection");
    return {true,*requested,{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
