// 原建设作者合同的纯校验；不把目录存在当作当前施工权或已完成工程。
#pragma once
#include "../economic_facility_provider.hpp"
#include "facility_construction_model.hpp"

namespace hanmo::resident {
// 只接受同一完整内容锁、真实 ProductionAsset 和原前后设施定义。
// 工人能力、当前权利、实际材料/支付和 S0 工期分别由开工/期间核验。
Status validate_facility_construction_catalog(
    const FacilityConstructionCatalog &, const CatalogIdentity &,
    const CommonCatalog &, const ProductionManagementCatalog &,
    const EconomicFacilityCatalog &);
// 借用原锁定行；结果与 catalog 同寿命，不持久化指针。
Outcome<const ConstructionTerms *>
read_facility_construction_terms(const FacilityConstructionCatalog &,
                                 const EconomicRuleRef &);
} // namespace hanmo::resident
