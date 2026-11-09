// 弟子关系与原死亡相接的纯依据；它不代表指定、本人接续同意、死亡知情或财产处分权。
#pragma once
#include "resident/kernel/common_fact_prefix.hpp"
#include "family_life_exact.hpp"
#include "family_apprenticeship.hpp"
#include "actor_death_read.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
struct ApprenticeshipLegacyBasis {
    WorldIdentity source_identity;
    StableId predecessor_actor_id,successor_actor_id,bond_fact_id,skill_id;
    StableId death_fact_id,casualty_fact_id,before_fact_id;
    Integer accepted_tick=0,death_tick=0,observed_tick=0;std::optional<Integer> successor_birth_tick;
    std::optional<VerifiedDeathView> verified_death;
};
// 所有借用均来自一个已通过原人物/出生/军事/生命/师承完整史的同候选；不递归验证 World。
struct ApprenticeshipLegacyBasisContext {
    const CommonStore& common;
    const CommonCatalog& catalog;
    const FamilyApprenticeshipState& apprenticeships;
    const FamilyDeathState& deaths;
    const FamilyBirthState* births=nullptr;
    FamilyDeathOriginRead death_origin;
    const FamilyContinuationPeopleRead4* complete_people=nullptr;
};
// 只认调用者指定的原关系，不自动挑选继任者；当前处境与独立本人许可还须另核。
Outcome<ApprenticeshipLegacyBasis> read_apprenticeship_legacy_basis(const ApprenticeshipLegacyBasisContext&,const StableId& bond_fact_id,const StableId& predecessor,const StableId& successor);
// 消费方先核自己的真实payload.tick；后来退出/死亡不能倒改在该实际事实前成立的依据。
Outcome<ApprenticeshipLegacyBasis> read_apprenticeship_legacy_basis_before_fact(const ApprenticeshipLegacyBasisContext&,const StableId& bond_fact_id,const StableId& predecessor,const StableId& successor,Integer tick,const StableId& actual_before_fact);
Outcome<ApprenticeshipLegacyBasis> read_apprenticeship_legacy_basis_at_original_prefix(const ApprenticeshipLegacyBasisContext&,const StableId& bond,const StableId& predecessor,const StableId& successor,Integer tick,const CommonActualFactPrefix&);

}
