// B1 同一次成立候选的真实预约→实际投入。独立 offer/consent 不调用本口。
#pragma once
#include "foundation_asset_candidate.hpp"
namespace hanmo::resident {
struct FoundationPreparedOrganizationContext;
struct PendingFoundationAssetPreparation {
  std::size_t row_index = 0;
};
struct PendingFoundationAssetFulfillment {
  std::size_t row_index = 0;
};
// 只在原 S6 决定前、完整候选内准备；晚失败必须连本次预约整候选弃置。
Outcome<PendingFoundationAssetPreparation>
prepare_foundation_assets_in_candidate(FoundationAssetWriteContext &,
                                       const SourceRef &actual_consent,
                                       const MomentOwnerSource &actual_intent,
                                       Integer terms_revision);
// 原 S6 body 已存在后才接受真实 org-owned 目标容器。容量/地点/用途与
// 初始 Grant 由原专业创建/政治门另验，不能在这里改 donor 容器 Owner。
// 先核原同意向决定/安装的真实身份 cut；它不替完整成立规则，晚失败仍
// 整候选弃置。冷 S6 登记必须反向覆盖决定<安装<本履行，不能脱离它发布。
Outcome<PendingFoundationAssetFulfillment>
fulfill_foundation_assets_in_candidate(
    FoundationAssetWriteContext &, const PendingFoundationAssetPreparation &,
    const FoundationPreparedOrganizationContext &,
    const MomentOwnerSource &actual_decision,
    const MomentOwnerSource &actual_installation,
    const StableId &actual_organization, const StableId &destination_wallet,
    const StableId &destination_inventory);
// 原 Common kind3 注册需真正同根 reader；无空表 provider、无递归当前资源核。
class NativeFoundationAssetReservationOwners final
    : public DomainActivityReservationOwnerRead {
public:
  explicit NativeFoundationAssetReservationOwners(
      const FoundationAssetReadContext &read)
      : read_(read) {}
  Outcome<ReservationOwnerView>
  read_domain_activity_reservation_owner(const ReservationRef &) const override;

private:
  const FoundationAssetReadContext &read_;
};
} // namespace hanmo::resident
