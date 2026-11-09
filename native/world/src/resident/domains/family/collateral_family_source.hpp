// 旁支只由已记录共同祖先及两条真实亲代路径证明；血缘不等于认可、本人同意或承接权。
#pragma once
#include "biological_legacy_basis.hpp"
namespace hanmo::resident {
struct CollateralFamilySource {
    WorldIdentity source_identity;
    StableId first_actor_id,second_actor_id,common_ancestor_actor_id,before_fact_id;
    Integer observed_tick=0;
    // 两条路径均从共同祖先到本人；每条边保留原Installed或真实Birth的明确来源标签。
    std::vector<BiologicalChildOrigin> first_descent,second_descent;
};
// 同候选完整Common/Creation/Family/Birth/Lineage/Life先验；只供权威规则，不向任意观察者公开族谱。
// 空值表示未能证明旁支，包含未知父母或直系；不能据此断言两人没有血缘。
Outcome<std::optional<CollateralFamilySource>> read_collateral_family_source(const BiologicalLegacyBasisContext&,const StableId& first,const StableId& second);
// 消费方另核自己的实际payload.tick；后出生者及后来关系来源不能倒填这个真实事实前截面。
Outcome<std::optional<CollateralFamilySource>> read_collateral_family_source_before_fact(const BiologicalLegacyBasisContext&,const StableId& first,const StableId& second,Integer observed_tick,const StableId& actual_before_fact);
}
