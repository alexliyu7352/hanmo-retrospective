// 实际占用提供者的逐期照护Action，儿童不是签约/劳动参与者；结束后外层释放原Action和等待对白。
#pragma once
#include "child_care_relief.hpp"
namespace hanmo::resident {
struct ChildCareReliefActionQuote {WorldIdentity identity;StableId accepted_id,provider_actor_id,child_actor_id,place_id;Integer duration=0,period_index=0,food_per_period=0;};
struct ChildCareReliefEffect {StableId fact_id,child_actor_id,code;bool success=false,already_completed=false;};
Outcome<ChildCareReliefActionQuote> preview_child_care_relief_action(const CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& provider_actor_id,const StableId& accepted_id);
Status start_child_care_relief_action_in_candidate(CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const ActionCoreView&,const ChildCareReliefActionQuote&);
Outcome<ChildCareReliefEffect> finish_child_care_relief_action_in_candidate(CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const ActionCoreView&);
Outcome<ChildCareReliefEffect> cancel_child_care_relief_action_in_candidate(CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& actor_id,const ActionCoreView&);
Outcome<ChildCareReliefEffect> interrupt_child_care_relief_action_in_candidate(CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const ActionCoreView&);
// 已登记死亡而当前未开工时，精确终止自己的志愿合同；原出生预约仍由原Birth账负责。
Outcome<StableId> close_child_care_relief_after_death_in_candidate(CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& accepted_id);
}
