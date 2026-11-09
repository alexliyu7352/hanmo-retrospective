// 产权源纯读。调用方仍须另核场所使用、建造处分、当事人行动资格与原债。
#pragma once
#include "../economic_facility_provider.hpp"
#include "production_asset_lifecycle.hpp"

namespace hanmo::resident {
struct FacilityConstructionAdoptionContext;
struct ProductionAssetLifecycleReadContext {
  const EconomicFacilityInstallationReadContext &installation;
  const ProductionAssetLifecycleState &state;
  const CatalogIdentity &identity;
  const CurrentMomentRead &clock;
};
// 借真实私产当事人的原钱包，既不发付款权，也不允许第三方代为同意。
struct ProductionAssetPrivateParties {
  const ActorCoreView *seller = nullptr, *buyer = nullptr;
  const WalletView *payer = nullptr, *payee = nullptr;
};
Outcome<ProductionAssetPrivateParties> read_production_asset_private_parties(
    const CommonStore &, const StableId &seller, const StableId &buyer);
// 首个原设施分支：真实安装为初始头，逐原 Common 顺序重放双方同意与
// 实际交割；缺少源、重复使用旧报价或钱腿不符均拒。不证明其他使用权。
Outcome<ProductionAssetTitleRead>
read_current_production_asset_title(const ProductionAssetLifecycleReadContext &,
                                    const StableId &asset_ref);
// 仅真实建设采用这一具名排他 cut，不开放任意 Moment 的查询通行口。
// 先全核当前产权历史及原 construction adoption，再借当时实际头；仍须
// 另外验证本人建造同意、场所政策和完整期间的权变，不能单凭它开工。
Outcome<ProductionAssetTitleRead>
read_production_asset_title_before_construction(
    const ProductionAssetLifecycleReadContext &,
    const FacilityConstructionAdoptionContext &, const MomentOwnerSource &);
// 精确核对借用结果的真实主体；空/失败 read 不变成默认 actor，也不解引用空值。
bool production_asset_title_owned_by(const ProductionAssetTitleRead &,
                                     Owner::Kind, std::string_view) noexcept;
// 精确比较当前实际头，不分配别名 SourceRef，也不将 source 形状当授权。
bool production_asset_title_source_equal(const ProductionAssetTitleRead &,
                                         const MomentOwnerSource &) noexcept;
} // namespace hanmo::resident
