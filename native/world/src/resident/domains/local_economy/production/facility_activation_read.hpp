// 实际应用对后继容量的单向依赖 cut；不是可由 caller 自填的当前授权。
#pragma once
#include "facility_activation_original_work.hpp"
#include "production_asset_construction_authority.hpp"
namespace hanmo::resident {
struct FacilityActivationReadContext {
  const FacilityConstructionServiceContext &construction;
  const ProductionAssetLifecycleReadContext &assets;
  const ProductionWorkApplicationsState &applications;
};
// 原实际应用头、工料、冻结定义、本人同意、完整用途窗口及不重复应用。
// 借同 job 真行；不重入消费本结果的 capacity/holder 验证。整体 World
// 出口仍须反向核真实容量采用及物理处置，不能只本口成功就发布应用。
Outcome<const AppliedFacilityActivation *>
read_facility_activation_for_capacity_dependency(
    const FacilityActivationReadContext &,
    const FacilityActivationReceiptRef &);
// 整体出口的反向覆盖：每个应用必须被原唯一容量链消费，并以其实际
// 前驱定义升级；先核原 provider 的物理/留存规则，不从 receipt 自证容量。
Status validate_facility_activation_capacity_adoptions(
    const FacilityActivationReadContext &,
    const EconomicFacilityProviderContext &);
} // namespace hanmo::resident
