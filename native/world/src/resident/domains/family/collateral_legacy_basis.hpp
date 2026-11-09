// 真实死亡后的旁支关系资格；认可和谱系均先于原死因，仍不授控制许可或可玩处境。
#pragma once
#include "resident/kernel/common_fact_prefix.hpp"
#include "family_life_exact.hpp"
#include "collateral_control_sources.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
struct CollateralLegacyBasis {
    CollateralRecognitionBasis relation;
    StableId predecessor_actor_id,successor_actor_id,death_fact_id,casualty_fact_id,before_fact_id;
    Integer death_tick=0;std::optional<Integer> successor_birth_tick;Integer observed_tick=0;
    std::optional<VerifiedDeathView> verified_death;
};
Outcome<CollateralLegacyBasis> read_collateral_legacy_basis(const CollateralControlDecisionContext&,const StableId& exact_recognition,const StableId& predecessor,const StableId& successor);
// 仅原family_legacy:/family_legacy_control_activated/family_legacy实际头；完整原关系、死亡先验。
Outcome<CollateralLegacyBasis> read_collateral_legacy_basis_before_fact(const CollateralControlDecisionContext&,const StableId& exact_recognition,const StableId& predecessor,const StableId& successor,Integer tick,const StableId& actual_activation);
Outcome<CollateralLegacyBasis> read_collateral_legacy_basis_at_original_prefix(const CollateralControlDecisionContext&,const StableId& recognition,const StableId& predecessor,const StableId& successor,Integer tick,const CommonActualFactPrefix&);

}
