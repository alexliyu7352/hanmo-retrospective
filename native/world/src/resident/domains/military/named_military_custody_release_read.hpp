// 看管关闭的完整权威与 S1 placement 叶分开：只有完整原源读口能够解除命令门。
#pragma once
#include "named_military_custody_chain.hpp"
#include "named_military_custody_destination.hpp"
#include "named_military_custody_placement.hpp"
#include "resident/domains/travel/actor_spatial_history.hpp"
namespace hanmo::resident {
struct NamedMilitaryCustodyReleaseContext {
  NamedMilitaryCustodyContext custody;
  const CommonCatalog &common_catalog;
  const TravelCatalog &travel;
  NamedMilitaryCustodyTerritorySources territory;
  const MovementExactState &movement;
  const SpatialCarrierState &carriers;
  const TransportRegistrationState &registrations;
  const AuthoredActorFoundationState &foundation;
  const ActorFoundationCatalog &actors;
  const SpatialCarrierSources &spatial;
  ActorBattleSpatialSources battle;
  const NamedMilitaryCustodyPlacementRead &placements;
  const MilitarySpatialHandoffState *military_handoffs = nullptr;
  const FamilyCatalog *family_catalog = nullptr;
  const BirthPlacementRead *births = nullptr;
  const MilitaryActionPlacementRead *military_actions = nullptr;
  ExactArithmeticBudget budget;
  // 冷历史与热采用共借原 Field 退出/死俘交接读口，不造旧 Battle 替身。
  const FieldBattleExitPlacementRead *field_exits = nullptr;
};
Status validate_named_military_custody_release_sources(const NamedMilitaryCustodyReleaseContext &);
// 永久历史必须按原关闭头取同根正文，不能借 current-null 猜过去已经释放。
Outcome<const NamedMilitaryCustodyRelease *> read_original_named_military_custody_release(
    const NamedMilitaryCustodyReleaseContext &, const SourceRef &);
class NamedMilitaryCustodyReleaseRead {
public:
  virtual ~NamedMilitaryCustodyReleaseRead() = default;
  virtual bool bound_to(const CommonStore &,const MilitaryState &) const noexcept = 0;
  // null 只表示真实尚未关闭或已发生新的俘获；坏关闭来源必须显式失败。
  virtual Outcome<const NamedMilitaryCustodyRelease *> read_current_named_custody_release(
      const StableId &actor_id) const = 0;
};
class OriginalNamedMilitaryCustodyReleaseRead final : public NamedMilitaryCustodyReleaseRead {
  NamedMilitaryCustodyReleaseContext context_;
public:
  explicit OriginalNamedMilitaryCustodyReleaseRead(NamedMilitaryCustodyReleaseContext context)
      : context_(context) {}
  bool bound_to(const CommonStore &,const MilitaryState &) const noexcept override;
  Outcome<const NamedMilitaryCustodyRelease *> read_current_named_custody_release(
      const StableId &actor_id) const override;
};
} // namespace hanmo::resident
