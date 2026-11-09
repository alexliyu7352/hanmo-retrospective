// 唯一出生事实扩展原谱系；只有已核动态身份可补入，收养和改名不会改写血缘或先天来源。
#pragma once
#include "family_birth.hpp"
namespace hanmo::resident {
struct BirthLineageProjection {
    OrderedTable<BiologicalOrigin> origins;
    OrderedTable<OrderedTable<Integer>> innate_levels;
    OrderedTable<StableId> birth_sources;
};
// 前置为完整安排/出生生命周期已验证；此叶独立反核每个创建头、原双方与不可变身份。
Outcome<BirthLineageProjection> read_birth_lineage_projection(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&);
// 只供权威资格使用，不是任意观察者能读取的公开家庭树。
Outcome<BiologicalRelationRead> read_birth_biological_relation(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const StableId& first,const StableId& second);
Outcome<InnateParentSource> read_birth_innate_parent(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const StableId& actor,const StableId& family_id);
}
