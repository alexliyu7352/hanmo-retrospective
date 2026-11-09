// 混合家庭亲缘只消费已验证的原血缘与原收养来源；未知生物亲代永远不会被填成无血缘。
#pragma once
#include "family_birth_lineage.hpp"
#include "family_model.hpp"
#include <map>
namespace hanmo::resident {
// Family完整史先验；历史边界只能是实际Common事实，空边界读取当前真实前缀。
Outcome<std::map<StableId,StableId>> read_birth_adoptive_parents(const CommonStore&,const FamilyState&,const StableId& before_fact={});
// 原血缘不变，收养图由调用者提供确切历史前缀；拒已知直系、同父母手足与混合祖先环。
Status validate_birth_known_family_pair(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const std::map<StableId,StableId>& adoptive_parents,const StableId& first,const StableId& second);
struct BornCharacterSourceContext;
Status validate_birth_known_family_pair4(const BornCharacterSourceContext&,const std::map<StableId,StableId>& adoptive_parents,const StableId& first,const StableId& second);

}
