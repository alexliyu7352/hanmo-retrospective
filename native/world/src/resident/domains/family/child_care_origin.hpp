// 已出生儿童原照护责任的具名纯读；不授监护权、知识或原预约粮处分权。
#pragma once
#include "family_birth.hpp"
#include "actor_death_read.hpp"
namespace hanmo::resident {
struct ChildCareOriginRead {
    WorldIdentity source_identity;
    StableId child_actor_id,birth_fact_id,accepted_fact_id,original_caregiver_actor_id,original_payer_actor_id;
    StableId last_care_fact_id,last_care_action_id,last_care_code;
    StableId place_id,source_inventory_id,food_item_id,pending_action_id,caregiver_death_fact_id,child_death_fact_id,before_fact_id;
    std::vector<StableId> biological_parent_ids,living_parent_ids;
    Integer observed_tick=0,birth_tick=0,completed_periods=0,prepaid_periods=0,food_per_period=0,remaining_reserved_food=0;
    bool original_schedule_closed=false;
};
// 同候选完整Birth/Life/Military已验为前置；当前口另核原预约仍精确存在，不把余额当已照护。
Outcome<ChildCareOriginRead> read_child_care_origin(const CommonStore&,const FamilyBirthState&,const FamilyDeathState&,const StableId& child_actor_id);
// 只允许实际family_care_relief:/family_care_relief_accepted/family_care_relief边界。
// 新接手域先核其payload.tick；本口的历史remaining是责任数，不证明今天或当年的现货可以转让。
Outcome<ChildCareOriginRead> read_child_care_origin_before_relief(const CommonStore&,const FamilyBirthState&,const FamilyDeathState&,const StableId& child_actor_id,Integer tick,const StableId& relief_fact_id);
}
