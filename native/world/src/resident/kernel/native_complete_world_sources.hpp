// Same-World construction graph. These are compiled owning boundaries, never
// author-supplied providers, mutable registries, checkpoint state or clones.
#pragma once
#include "native_action_time_sources.hpp"
#include "native_character_family_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_complete_world_registry.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
namespace hanmo::resident {
class NativeInteractionWorldRead;
class NativeOriginalSocialWorldRead;
class NativeMilitaryActionPlacementRead;
class NativeMilitaryWorldRemainingOwner;
class NativeStateNeedWorldOriginalOwner;
struct NavalHistoryAccess;
struct EconomicFacilityProviderContext;
struct CraftFacilityOccupancyContext;
class NativeHouseholdServiceLease;
class NativeHouseholdFoundationLease;

// The early graph has no Body/ActionTime callback. These original source leaves
// can be constructed before the consumers that use them. The object containing
// this view is immovable and outlives every construction callback and export.

struct NativeCompleteFamilyPrerequisiteExports {
  const SpatialCarrierSources &spatial_original;
  ActorBattleSpatialSources battle_spatial;
  const BirthPlacementRead *birth_placement = nullptr;
  const EnvironmentalSeaDeathSourceRead *sea = nullptr;
  const BornCharacterSourceRead *external_born = nullptr;
  const BirthDueBornSourceRead *birth_results = nullptr;
  const CraftJobReadContext *craft = nullptr;
  const DiscipleControlRelationContext *disciple = nullptr;
  const BiologicalControlDecisionContext *biological = nullptr;
  const CollateralControlDecisionContext *collateral = nullptr;
};
// The original Spatial/Sea/Craft/Control implementations belong here, owned by
// their domain construction. A missing implementation means no factory. Do not
// install an object whose business methods just reject or return empty success.
class NativeCompleteFamilyPrerequisiteOwner : public CheckpointAllocatedObject {
public:
  NativeCompleteFamilyPrerequisiteOwner() = default;
  virtual ~NativeCompleteFamilyPrerequisiteOwner() = default;
  NativeCompleteFamilyPrerequisiteOwner(
      const NativeCompleteFamilyPrerequisiteOwner &) = delete;
  NativeCompleteFamilyPrerequisiteOwner &
  operator=(const NativeCompleteFamilyPrerequisiteOwner &) = delete;
  NativeCompleteFamilyPrerequisiteOwner(
      NativeCompleteFamilyPrerequisiteOwner &&) = delete;
  NativeCompleteFamilyPrerequisiteOwner &
  operator=(NativeCompleteFamilyPrerequisiteOwner &&) = delete;
  virtual bool bound_to(const WorldBundle &) const noexcept = 0;
  // 先只验已存在的空间/身体前序；此阶段不得读尚未构造的 Family people。
  virtual Status
  validate_early_bindings(const NativeCompleteWorldEarlySources &) const = 0;
  // 仅唯一 Complete 的私有构造栈调用一次；原 owner 持有所有晚关系 context。
  // 返回前必须令 complete_people 指向传入的真实 people，不能借旧 World。
  virtual Status
  bind_family_sources(const NativeCharacterFamilyWorldRead &) = 0;
  virtual Status
  validate_bindings(const NativeCompleteWorldEarlySources &) const = 0;
  virtual const NativeCompleteFamilyPrerequisiteExports &
  sources() const noexcept = 0;
};

class NativeCompleteActionPrerequisiteOwner : public CheckpointAllocatedObject {
public:
  NativeCompleteActionPrerequisiteOwner() = default;
  virtual ~NativeCompleteActionPrerequisiteOwner() = default;
  NativeCompleteActionPrerequisiteOwner(
      const NativeCompleteActionPrerequisiteOwner &) = delete;
  NativeCompleteActionPrerequisiteOwner &
  operator=(const NativeCompleteActionPrerequisiteOwner &) = delete;
  NativeCompleteActionPrerequisiteOwner(
      NativeCompleteActionPrerequisiteOwner &&) = delete;
  NativeCompleteActionPrerequisiteOwner &
  operator=(NativeCompleteActionPrerequisiteOwner &&) = delete;
  virtual bool bound_to(const WorldBundle &) const noexcept = 0;
  virtual Status
  validate_bindings(const NativeCompleteWorldCharacterSources &) const = 0;
  // 中央接入 growth/tasks/travel 与三个原交互域；这里只交其余已选原域。
  // 各 source 必须由真实原 owner 提供，并由中央核对完整注册。
  // 重复或缺失均硬拒，不覆盖、不去重，不用空集合冒充已闭合。
  virtual const CheckpointVector<CompiledActionTimeOwnerBinding> &
  action_time_owners() const noexcept = 0;
  virtual std::shared_ptr<const MotionCargoReadDependencies>
  cargo_sources() const = 0;
};
struct NativeOrganizationResourceValidationReads;
class NativeEconomicInstalledFacilityAppliedRead;
class NativeEconomicFifoWorldScope;
class NativePoliticalWorldScope;

struct NativeCompleteDomainPrerequisiteExports {
  const EconomicFacilityProviderContext *facility = nullptr;
  const CraftFacilityOccupancyContext *craft_occupancy = nullptr;
  const HouseholdPurchaseContext *household_purchases = nullptr;
  const HouseholdRetailReadContext *household_retail = nullptr;
  const PopulationMilitaryReadContext *precise_population = nullptr;
  const NativeTransportInventoryEntryRead *transport_inventory_entries =
      nullptr;
  const TransportManifestHistorySourceRead *transport_manifest_history =
      nullptr;
  const PrivateShopOperationSourceContext *private_shop_operations = nullptr;
  const NavalHistoryAccess *military_naval_history = nullptr;
  std::shared_ptr<NativeMilitaryWorldRemainingOwner> military_remaining;
  std::shared_ptr<NativeStateNeedWorldOriginalOwner> state_need_original;
  // Only these original late writers are supplied here. Growth and Social
  // writers are actual native members of the central graph, not substitutes.
  CareLifeConditionWrite *care_life = nullptr;
  GrowthStartedParticipationWrite *growth_start = nullptr;
  // 实际P05自有容量与S5上下文来自同一不可移动owner，不因它存在强选经济09。
  const NativeHouseholdServiceLease *household_service = nullptr;
  // 真实前序各自拥有；不得 alias 回持本 Domain 而形成保活环。
  std::shared_ptr<const NativeEconomicInstalledFacilityAppliedRead>
      installed_original;
  std::shared_ptr<const NativeEconomicFifoWorldScope> market_original;
  // 仅原 Shop2 精确选型借出同一不可移动家户根；未选为空，不持久化。
  const NativeHouseholdFoundationLease *household_foundation = nullptr;
  // 真实 Foundation 的原运输业务，不是早期转发口或 report-only Task reader。
  const TransportRegistrationBusinessRead *household_transport = nullptr;
};
class NativeCompleteDomainPrerequisiteOwner : public CheckpointAllocatedObject {
public:
  NativeCompleteDomainPrerequisiteOwner() = default;
  virtual ~NativeCompleteDomainPrerequisiteOwner() = default;
  NativeCompleteDomainPrerequisiteOwner(
      const NativeCompleteDomainPrerequisiteOwner &) = delete;
  NativeCompleteDomainPrerequisiteOwner &
  operator=(const NativeCompleteDomainPrerequisiteOwner &) = delete;
  NativeCompleteDomainPrerequisiteOwner(
      NativeCompleteDomainPrerequisiteOwner &&) = delete;
  NativeCompleteDomainPrerequisiteOwner &
  operator=(NativeCompleteDomainPrerequisiteOwner &&) = delete;
  virtual bool bound_to(const WorldBundle &) const noexcept = 0;
  // This is a root/selection/construction check, not a whole source validator.
  // It must not read a holder until the private one-time bind has completed.
  virtual Status validate_bindings(const NativeCompleteWorldCoreSources &,
                                   bool writable) const = 0;
  virtual const NativeCompleteDomainPrerequisiteExports &
  sources() const noexcept = 0;
};

// All prerequisite owners and per-acquisition scopes must release their own
// payloads without invoking downstream holders or the Complete owner during
// destruction. Private back-links never authorize destructor callbacks.
// All eleven domain factory pairs have the same typed core, but are explicit
// finite slots. Each implementation constructs and owns its own actual-M
// snapshots. The Complete owner is already fully constructed, not published;
// initial construction must never call its public getters or acquisitions.
struct NativeCompleteDomainScopeFactories {
  using Read = Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>> (
          *)(const WorldBundle &, const NativeCompleteWorldCoreSources &,
             const NativeCompleteDomainPrerequisiteOwner &,
             const WorldMoment &);
  using Write = Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>> (*)(
      WorldBundle &, const NativeCompleteWorldCoreSources &,
      NativeCompleteDomainPrerequisiteOwner &, CompleteExactWorldBindingOwner &,
      const WorldMoment &, ExactPhaseWriteContext *, const DynamicStepInput *);
  Read read = nullptr;
  Write write = nullptr;
};
struct NativeCompleteWorldSourceGraphFactories {
  Outcome<std::unique_ptr<NativeCompleteFamilyPrerequisiteOwner>> (
      *family_prerequisites)(const NativeCompleteWorldEarlySources &) = nullptr;
  Outcome<std::unique_ptr<NativeCompleteActionPrerequisiteOwner>> (
      *action_prerequisites)(const NativeCompleteWorldCharacterSources &) =
      nullptr;
  Outcome<std::unique_ptr<NativeCompleteDomainPrerequisiteOwner>> (
      *domain_read)(const NativeCompleteWorldCoreSources &) = nullptr;
  Outcome<std::unique_ptr<NativeCompleteDomainPrerequisiteOwner>> (
      *domain_write)(const NativeCompleteWorldCoreSources &, WorldBundle &,
                     CompleteExactWorldBindingOwner &) = nullptr;
  NativeCompleteDomainScopeFactories economic, craft, repair, craft_facility,
      household, motion, state_need, political, task_work, military, director;
};
const NativeCompleteWorldSourceGraphFactories &
native_complete_world_source_graph_factories() noexcept;
// Production entry points called by the original compiled registry. They now
// construct real native members; missing domain construction remains named.
Outcome<std::unique_ptr<NativeCompleteWorldSourceOwner>>
make_native_complete_world_read_sources(const WorldBundle &,
                                        const CompleteExactWorldBindingOwner &);
Outcome<std::unique_ptr<NativeCompleteWorldSourceOwner>>
make_native_complete_world_write_sources(WorldBundle &,
                                         CompleteExactWorldBindingOwner &);
// Finite diagnostic only: linked construction is not selected-source readiness.
struct NativeCompleteSourceConstructionStatus {
  const char *name;
  bool read_constructor = false, write_constructor = false;
  const char *owner;
};
std::array<NativeCompleteSourceConstructionStatus, 14>
read_native_complete_source_construction_status() noexcept;
} // namespace hanmo::resident
