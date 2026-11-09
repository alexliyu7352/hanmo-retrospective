// 遗留关系来源纯读；证明原收养与真实死亡相接，不授予控制、同意或财产处分权。
#pragma once
#include "family_life_exact.hpp"
#include "family_death.hpp"
#include "family_model.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
struct FamilyLegacyAdoptionBasis {
    WorldIdentity checked_identity;
    StableId predecessor_actor_id,successor_actor_id,death_fact_id,casualty_fact_id,relation_fact_id;
    Integer death_tick=0;std::optional<Integer> successor_birth_tick;
    std::optional<VerifiedDeathView> verified_death;
};
// 调用者先验证完整Family与Military/Naval史；返回有限来源，不能把它当作本人接受。
Outcome<FamilyLegacyAdoptionBasis> read_family_legacy_adoption_basis(
    const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,
    const StableId& predecessor_actor_id,const StableId& successor_actor_id,const FamilyContinuationPeopleRead4* complete=nullptr);
}
