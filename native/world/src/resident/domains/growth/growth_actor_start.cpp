#include "growth_actor_start.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool id(const StableId& value){return !value.empty()&&value.size()<=256;}
bool has(const std::vector<StableId>& ids,const StableId& value){return std::find(ids.begin(),ids.end(),value)!=ids.end();}
bool unique(const std::vector<StableId>& ids,bool empty=false){if((ids.empty()&&!empty)||ids.size()>4096)return false;std::set<StableId> seen;for(const auto& value:ids)if(!id(value)||!seen.insert(value).second)return false;return true;}
Status policy_shape(const GrowthActorStartPolicy& policy){
    if(!policy.cards.enabled||policy.cards.definitions.empty()||policy.cards.definitions.size()>4096||!unique(policy.playable_actor_ids)||!unique(policy.initially_available_actor_ids)||!has(policy.initially_available_actor_ids,policy.default_actor_id))return common_failure("InvalidActorStartPolicy");
    std::set<StableId> targets;
    for(const auto& row:policy.cards.definitions.rows())if(!has(policy.playable_actor_ids,row.second.target_actor_id)||!targets.insert(row.second.target_actor_id).second)return common_failure("InvalidActorStartCardTarget",row.first);
    for(const auto& actor:policy.initially_available_actor_ids)if(!has(policy.playable_actor_ids,actor))return common_failure("InvalidActorStartInitialActor",actor);
    for(const auto& actor:policy.playable_actor_ids)if(!has(policy.initially_available_actor_ids,actor)&&!targets.count(actor))return common_failure("MissingActorStartCard",actor);
    return common_success();
}
std::vector<StableId> list(const BoundaryValue& value,const StableId& path){std::vector<StableId> result;const auto& values=boundary::array(value,path);if(values.empty()||values.size()>4096)boundary::fail("InvalidActorStartList",path);for(const auto& row:values)result.push_back(boundary::string(row,path));if(!unique(result))boundary::fail("InvalidActorStartList",path);return result;}
}

Outcome<GrowthActorStartPolicy> decode_growth_actor_start_policy(const BoundaryValue& definitions,const BoundaryValue& config,const CommonCatalog& common,const std::vector<StableId>& roster,const StableId& default_actor){try{
    using namespace boundary;
    fields(config,{"profile","card_ids","initially_available_actor_ids"},{},"actor_card_unlocks");
    const auto profile=get_string(config,"profile","actor_card_unlocks");if(profile!="completed_procurement_actor_cards_v1"&&profile!="completed_task_actor_cards_v2")unsupported("UnsupportedActorStartProfile","actor_card_unlocks/profile");
    GrowthActorStartPolicy policy;policy.cards.enabled=true;policy.cards.version=profile=="completed_task_actor_cards_v2"?2:1;policy.playable_actor_ids=roster;policy.default_actor_id=default_actor;
    policy.initially_available_actor_ids=list(field(config,"initially_available_actor_ids","actor_card_unlocks"),"actor_card_unlocks/initially_available_actor_ids");
    const auto selected=list(field(config,"card_ids","actor_card_unlocks"),"actor_card_unlocks/card_ids");object(definitions,"actor_cards");
    for(const auto& card_id:selected){
        const auto path="actor_cards/"+card_id;const auto& value=field(definitions,card_id,"actor_cards");
        fields(value,{"id","category","name_key","description_key","hint_key","icon_id","prerequisites","acquire_condition","effect_kind","effect_params","repeat_policy"},{},path);
        if(get_string(value,"id",path)!=card_id)fail("InvalidActorCardIdentity",path);
        if(get_string(value,"category",path)!="actor"||get_string(value,"effect_kind",path)!="unlock_actor")unsupported("UnsupportedActorCardEffect",path);
        if(get_string(value,"repeat_policy",path)!="once_per_actor_profile")unsupported("UnsupportedActorCardRepeat",path);
        const auto& prerequisites=field(value,"prerequisites",path);fields(prerequisites,{"kind"},{},path+"/prerequisites");if(get_string(prerequisites,"kind",path)!="current_player_completed_tasks")unsupported("UnsupportedActorCardPrerequisite",path);
        const auto& acquisition=field(value,"acquire_condition",path);fields(acquisition,{"kind"},{},path+"/acquire_condition");
        const auto& effect=field(value,"effect_params",path);fields(effect,{"target_actor_id"},{},path+"/effect_params");
        get_string(value,"icon_id",path);
        GrowthActorCardDefinition card;card.id=card_id;card.target_actor_id=get_string(effect,"target_actor_id",path);card.name_key=get_string(value,"name_key",path);card.description_key=get_string(value,"description_key",path);card.hint_key=get_string(value,"hint_key",path);card.condition=get_string(acquisition,"kind",path);
        if(!policy.cards.definitions.insert(card_id,std::move(card)))fail("DuplicateActorCardIdentity",path);
    }
    auto valid=validate_growth_actor_card_catalog(policy.cards,common);if(!valid.ok)return {false,{},valid.error};valid=policy_shape(policy);if(!valid.ok)return {false,{},valid.error};
    return {true,std::move(policy),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}

Status validate_growth_actor_start_selection(const GrowthActorStartPolicy& policy,const StableId& selected,const std::vector<StableId>& unlocked){
    const auto valid=policy_shape(policy);if(!valid.ok)return valid;
    if(!unique(unlocked,true))return common_failure("InvalidUnlockedActorIds");
    if(!has(policy.playable_actor_ids,selected))return common_failure("ActorNotInStartRoster",selected);
    return has(policy.initially_available_actor_ids,selected)||has(unlocked,selected)?common_success():common_failure("ActorCardLocked",selected);
}
}
