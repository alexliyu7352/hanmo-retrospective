// 私铺只借该同候选短命许可读；法律不反调Commerce或全接管责任校验。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_owned.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_profile.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_view.hpp"
#include "resident/domains/politics/licensing/political_shop_registry_rules.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class NativePrivateShopExactSources;
// 实际 owner 对象体走同作业内存账；成员深值仍由各原拥有者负责。
class PoliticalShopLicensingWorldAccess final
    : public CheckpointAllocatedObject {
public:
  static Outcome<std::unique_ptr<PoliticalShopLicensingWorldAccess>>
  // exact来源必须在本借读owner之后销毁；Law1必须不传，Law2必须真实同World。
  create(const WorldBundle &, const DynamicStepInput *phase = nullptr,
         const NativePrivateShopExactSources *exact = nullptr);
  PoliticalShopLicensingWorldAccess(const PoliticalShopLicensingWorldAccess &) =
      delete;
  PoliticalShopLicensingWorldAccess &
  operator=(const PoliticalShopLicensingWorldAccess &) = delete;
  const PoliticalShopLicensingContext &context() const { return context_; }
  const PoliticalPrivateShopPermissionAccess &permissions() const {
    return permissions_;
  }
  const PoliticalShopRegistryContext *registry_context() const {
    return registry_context_ ? &*registry_context_ : nullptr;
  }

private:
  explicit PoliticalShopLicensingWorldAccess(
      const WorldBundle &, const DynamicStepInput *phase = nullptr,
      const NativePrivateShopExactSources *exact = nullptr);
  friend Status
  initialize_political_shop_licensing_world_in_candidate(WorldBundle &);
  std::optional<MilitaryAccess> military_;
  std::optional<PoliticalShopControlContext> control_;
  InteractionAccess interaction_;
  // 原始法律context不含接册回调，纯接册读可检查原册而不递归完整Shop。
  PoliticalShopLicensingContext original_context_;
  std::optional<PoliticalShopRegistryContext> registry_context_;
  std::optional<PoliticalShopRegistryAccess> registry_;
  PoliticalShopLicensingContext context_;
  PoliticalPrivateShopPermissionAccess permissions_;
};
// 仅原新局初始化阶段调用，完整Common和新法律段必须由调用方同候选撤回。
Status initialize_political_shop_licensing_world_in_candidate(WorldBundle &);
Status validate_political_shop_licensing_world(const WorldBundle &);
Outcome<OwnedPoliticalShopLicensingQuote>
prepare_political_shop_licensing_world_owned(
    const WorldBundle &, const StableId &,
    const PoliticalShopLicensingCommand &);
Outcome<PoliticalShopLicensingQuote>
prepare_political_shop_licensing_world(const WorldBundle &, const StableId &,
                                       const PoliticalShopLicensingCommand &);
Outcome<OwnedPoliticalShopLicensingEffects>
commit_political_shop_licensing_world_owned(
    WorldBundle &, const StableId &, const PoliticalShopLicensingCommand &,
    const PoliticalShopLicensingQuote &);
// Bare legacy ABI only; the original dispatcher uses the owned result.
Outcome<std::vector<StableId>>
commit_political_shop_licensing_world(WorldBundle &, const StableId &,
                                      const PoliticalShopLicensingCommand &,
                                      const PoliticalShopLicensingQuote &);
Outcome<BoundaryValue>
query_political_shop_licensing_world(const WorldBundle &, const StableId &place,
                                     Integer cursor = 0);
} // namespace hanmo::resident
