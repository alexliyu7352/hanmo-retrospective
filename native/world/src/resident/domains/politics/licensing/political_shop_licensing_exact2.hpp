#pragma once
#include "political_shop_licensing.hpp"
#include "resident/domains/organization/authority_basis.hpp"
namespace hanmo::resident {
struct PoliticalShopLicensingActualSource2 {
  const FactRecord *head = nullptr;
  const WorldMoment *occurred_at = nullptr;
};
// 仅原六种实际许可事实；不回完整许可/成员/Basis，不能用裸tick造发生M。
Outcome<PoliticalShopLicensingActualSource2>
read_political_shop_licensing_actual_source2(
    const PoliticalShopLicensingContext &, const SourceRef &);
// 仅本域实际六头或原Shop3实际opening；后者只原row/M，不回full许可。
Outcome<const WorldMoment *> read_political_shop_permission_boundary_moment2(
    const PoliticalShopLicensingContext &, const SourceRef &);
// 写入前借唯一实际时钟；旧1返回空，显式2返回真实M，未知选型拒绝。
Outcome<std::optional<WorldMoment>> read_political_shop_licensing_actual_now2(
    const PoliticalShopLicensingContext &);
Outcome<std::optional<WorldMoment>> read_political_shop_licensing_write_moment(
    const PoliticalShopLicensingContext &);
Status validate_political_shop_licensing_time_layout(
    const PoliticalShopLicensingState &);
Status
visit_political_shop_licensing_checkpoint2(CodecVisitor &,
                                           PoliticalShopLicensingState &);
Status
emit_political_shop_licensing_projection2(ProjectionWriter &, std::string_view,
                                          const PoliticalShopLicensingState &);
Outcome<BoundaryValue>
encode_political_shop_licensing2(const PoliticalShopLicensingState &);
Outcome<PoliticalShopLicensingState>
decode_political_shop_licensing2(const BoundaryValue &);
class NativeShopLicensingAuthoritySourceRead2 final
    : public ActualShopLicensingAuthoritySourceRead {
  const PoliticalShopLicensingContext &x_;

public:
  explicit NativeShopLicensingAuthoritySourceRead2(
      const PoliticalShopLicensingContext &x)
      : x_(x) {}
  bool bound_to(const CommonStore &c,
                const OrganizationState &s) const noexcept override {
    return &c == &x_.common && &s == &x_.organizations &&
           x_.state.version == 2 && x_.clock;
  }
  Outcome<WorldMoment> read_organization_authority_now() const override;
  Outcome<ActualShopLicensingAuthoritySourceView>
  read_actual_shop_authority2(const MomentOwnerSource &) const override;
};
} // namespace hanmo::resident
