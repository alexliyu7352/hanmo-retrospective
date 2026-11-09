// 原 LC01 只读静态定义，借实际作者行，不提供第二份服务条款或运行准入。
#pragma once
#include "growth_life_definition_ref.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct GrowthCatalog;
struct GrowthCourse;
struct GrowthActivityDefinition;
struct GrowthStudyDefinitionRead1 {
  const StableId *role_id = nullptr;
  const GrowthCourse *course = nullptr;
};
struct GrowthRestDefinitionRead1 {
  const StableId *role_id = nullptr;
  const GrowthActivityDefinition *activity = nullptr;
};
// 只判原三版本局部组合；完整同Prepared身份及Growth根地址由中央holder核验。
Status validate_growth_life_definition_selection1(const CatalogIdentity &);
Outcome<GrowthLifeDefinitionRef1>
decode_growth_life_definition_ref1(const BoundaryValue &);
// 已安装目录所有显式地址的正版本/唯一性；未被LC01引用的旧空地址保持合法。
Status validate_growth_life_definition_rows1(const GrowthCatalog &);
Outcome<GrowthStudyDefinitionRead1>
read_growth_study_definition1(const GrowthCatalog &, const CatalogIdentity &,
                              const StableId &id, Integer version);
Outcome<GrowthRestDefinitionRead1>
read_growth_rest_definition1(const GrowthCatalog &, const CatalogIdentity &,
                             const StableId &id, Integer version);
// 新静态选择才允许作者definition_ref；原无选择loader继续拒绝此新增字段。
Outcome<GrowthCatalog>
decode_growth_life_catalog1(const BoundaryValue &, const CommonCatalog &,
                            const CatalogIdentity &, bool regional_life = false,
                            bool innate_learning = false);
} // namespace hanmo::resident
