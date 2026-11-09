// 建设施工采用的原依赖 cut；不授予施工权，不把有开工行当成已供工。
#pragma once
#include "facility_construction_catalog.hpp"

namespace hanmo::resident {
struct FacilityConstructionAdoptionContext {
  const CommonStore &common;
  const CommonCatalog &common_catalog;
  const CatalogIdentity &identity;
  const ProductionManagementCatalog &assets;
  const EconomicFacilityCatalog &facilities;
  const FacilityConstructionCatalog &catalog;
  const FacilityConstructionState &state;
  const CurrentMomentRead &clock;
};
// 校原条款、唯一 Action 身份、真实成本行及 Common 先后，不回入后继
// S0 信用、S4 身体或 S6 权源校验。那些消费者可据此核同一真实采用 cut，
// 最终开工/稳定世界仍须完整核身体、地点和当前权利，不能只调用本口。
// 借用值仅与此 context 同 job、同根寿命有效，不能写进任何持久根。
Outcome<const FacilityConstruction *>
read_facility_construction_adoption_for_original_dependency(
    const FacilityConstructionAdoptionContext &, const MomentOwnerSource &);
// 原 ID 的便利借读仍走上面唯一规则；临时三字段源先按同 job 精确预留。
Outcome<const FacilityConstruction *>
read_facility_construction_by_id_for_original_dependency(
    const FacilityConstructionAdoptionContext &, const StableId &);
} // namespace hanmo::resident
