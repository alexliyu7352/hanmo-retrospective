#include "native_complete_domain_prerequisites.hpp"
#include "native_field_group_world.hpp"
#include "native_political_world_scope.hpp"
#include "resident/domains/local_economy/economic_facility_installed_sources.hpp"
#include "resident/domains/local_economy/economic_payment_world_scope.hpp"
#include "resident/domains/local_economy/household_world_scope.hpp"
#include "resident/domains/local_economy/market_history/economic_fifo_world_scope.hpp"
#include "resident/domains/military/native_field_military_consumers.hpp"
#include "resident/domains/military/native_field_military_remaining.hpp"
#include "world_bundle.hpp"
#include <new>
namespace hanmo::resident {
namespace {
// 每次实际 phase 构造独立前序；P1 与 Installed 由 FIFO 自己的别名保活。
Outcome<std::shared_ptr<NativeEconomicFifoWorldScope>> make_market_original(
    const NativeCompleteWorldCoreSources &core,
    std::shared_ptr<const NativeEconomicInstalledFacilityAppliedRead> installed,
    const DynamicStepInput *phase,
    EconomicWorkSourcesState *mutable_production) {
  const auto &w = core.characters.early.world;
  if (!w.catalog || !w.catalog->growth_market_history ||
      w.catalog->growth_market_history->selected_version != 2)
    return {false,
            {},
            common_failure("CompleteOriginalMarketSelectionRequired").error};
  NativeEconomicFifoWorldLateSources late;
  if (w.catalog->production_cycles) {
    auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    NativePoliticalWorldLateRead political_late;
    political_late.installation =
        installed ? &installed->installation() : nullptr;
    political_late.actual_phase = phase;
    auto political = NativePoliticalWorldScope::create_read(w, core, now.value,
                                                            political_late);
    if (!political.ok)
      return {false, {}, std::move(political.error)};
    const auto &owner = political.value;
    late.political = std::shared_ptr<const PoliticalExactReadLease>(
        owner, &owner->political_read_lease());
    late.installed = std::move(installed);
    if (const auto *takeover = owner->takeover_context())
      late.takeover =
          std::shared_ptr<const PoliticalTakeoverContext>(owner, takeover);
    NativeEconomicPaymentWorldSources payment_sources;
    payment_sources.political = owner;
    auto original_payments =
        NativeEconomicPaymentWorldScope::create(w, std::move(payment_sources));
    if (!original_payments.ok)
      return {false, {}, std::move(original_payments.error)};
    auto payment_owner = checkpoint_share(std::move(original_payments.value));
    if (mutable_production)
      late.original_payments =
          std::shared_ptr<const EconomicOriginalPaymentRead>(
              payment_owner, &payment_owner->read());
  }
  auto market = NativeEconomicFifoWorldScope::create(core, std::move(late),
                                                     phase, mutable_production);
  if (!market.ok)
    return {false, {}, std::move(market.error)};
  return {true, checkpoint_share(std::move(market.value)), {}};
}
// 固定地址持有真实初装与家户子图；原holders只在中央完成一次绑定后消费。
class DomainPrerequisites final : public NativeCompleteDomainPrerequisiteOwner {
  const NativeCompleteWorldCoreSources &core_;
  const WorldBundle &world_;
  bool writable_;
  std::shared_ptr<NativeEconomicInstalledFacilityAppliedRead> installed_;
  std::optional<EconomicFacilityProviderContext> facility_;
  std::shared_ptr<NativeEconomicFifoWorldScope> market_;
  std::unique_ptr<NativeHouseholdWorldScope> household_;
  std::unique_ptr<NativeFieldGroupWorld> field_group_;
  NativeCompleteDomainPrerequisiteExports exports_;
  DomainPrerequisites(const NativeCompleteWorldCoreSources &core, bool writable)
      : core_(core), world_(core.characters.early.world), writable_(writable) {}

public:
  static Outcome<std::unique_ptr<NativeCompleteDomainPrerequisiteOwner>>
  create(const NativeCompleteWorldCoreSources &core,
         WorldBundle *mutable_world) {
    const bool writable = mutable_world != nullptr;
    const auto &w = core.characters.early.world;
    if ((mutable_world && mutable_world != &w) || !w.catalog ||
        (!w.catalog->household_purchases && !w.catalog->economic_facilities))
      return {
          false,
          {},
          common_failure("CompleteSelectedDomainOriginalConstructionRequired",
                         {}, ErrorKind::unsupported_capability)
              .error};
    auto out = std::unique_ptr<DomainPrerequisites>(
        new DomainPrerequisites(core, writable));
    const auto &p = *w.catalog;
    if (p.economic_facilities) {
      if (!w.economic_facility_capacity_sources ||
          !p.production_facility_conditions ||
          !w.production_facility_conditions || !p.local_economy ||
          !p.local_economy->production_management || !p.estate ||
          bool(p.private_shops) != w.private_shops_seed_complete ||
          !w.facility_claims)
        return {false,
                {},
                common_failure("CompleteEconomicFacilitySelectedRootsRequired")
                    .error};
      auto now = w.actual_phase_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      out->installed_ = checkpoint_share(
          std::unique_ptr<NativeEconomicInstalledFacilityAppliedRead>(
              new NativeEconomicInstalledFacilityAppliedRead(
                  EconomicFacilityInstallationReadContext{
                      w.common,
                      *p.economic_facilities,
                      *w.economic_facility_capacity_sources,
                      *p.local_economy->production_management,
                      *p.estate,
                      p.private_shops.get(),
                      now.value,
                      {}},
                  *p.production_facility_conditions,
                  *w.production_facility_conditions, *w.facility_claims)));
      // 真实Installed有限来源先验证全部初装/逆覆盖，不能把空动态表当未选。
      auto valid = out->installed_->validate_bindings();
      if (!valid.ok)
        return {false, {}, std::move(valid.error)};
      out->facility_.emplace(EconomicFacilityProviderContext{
          w.common,
          *p.economic_facilities,
          *w.economic_facility_capacity_sources,
          *p.local_economy->production_management,
          *p.estate,
          w.estate,
          p.private_shops.get(),
          w.private_shops,
          *p.production_facility_conditions,
          *w.production_facility_conditions,
          *w.facility_claims,
          core.facility_holders,
          *out->installed_,
          now.value,
          {}});
      out->exports_.facility = &*out->facility_;
      out->exports_.installed_original = out->installed_;
    }
    // P1 原读可先于生产 Calendar origin 持有同根 binding；不提前构政治写者。
    // 独立前序由下游 shared alias 保活，且没有任何反向父 owner 所有权。
    if (p.growth_market_history &&
        p.growth_market_history->selected_version == 2) {
      auto market =
          make_market_original(core, out->installed_, nullptr,
                               mutable_world && mutable_world->production_cycles
                                   ? &*mutable_world->production_cycles
                                   : nullptr);
      if (!market.ok)
        return {false, {}, std::move(market.error)};
      out->market_ = std::move(market.value);
      out->exports_.market_original = out->market_;
    }
    if (p.household_purchases) {
      NativeHouseholdWorldLateSources late;
      late.transport_business = core.characters.early.transport_business;
      if (out->market_)
        late.actual_market = std::shared_ptr<const NativeEconomicFifoRead>(
            out->market_, &out->market_->fifo());
      auto household = NativeHouseholdWorldScope::create(core, std::move(late));
      if (!household.ok)
        return {false, {}, std::move(household.error)};
      out->household_ = std::move(household.value);
      const auto &foundation = out->household_->foundation();
      out->exports_.household_service = &foundation.service();
      out->exports_.household_purchases = &foundation.service().household();
      out->exports_.household_retail = &foundation.service().retail();
      out->exports_.private_shop_operations = foundation.operations();
      if (core.characters.early.transport_business)
        out->exports_.household_transport = &foundation.transport();
      if (foundation.operations())
        out->exports_.household_foundation = &foundation;
    }
    // Field 的原 Army/库存/M-PASSAGE 图先构好；不经尚未发布的 Complete
    // getters。
    if (p.field_group_passage) {
      auto group = NativeFieldGroupWorld::create(w, core, mutable_world);
      if (!group.ok)
        return {false, {}, std::move(group.error)};
      out->field_group_ = std::move(group.value);
      out->exports_.transport_inventory_entries =
          &out->field_group_->inventory_entries();
      out->exports_.transport_manifest_history =
          &out->field_group_->manifests();
      NativeFieldMilitaryRemainingDependencies remaining;
      // 未选择 siege/return 的有限 fixture 不造消费者；任何真实后果由原
      // calendar 拒绝并撤候选。
      remaining.release_action =
          &release_native_field_military_action_in_candidate;
      remaining.adopt_formation_locations =
          &adopt_native_field_formation_locations_in_candidate;
      auto military = make_native_field_military_remaining_owner(
          w, core.characters.early.complete, std::move(remaining));
      if (!military.ok)
        return {false, {}, std::move(military.error)};
      out->exports_.military_remaining = std::move(military.value);
    }
    auto valid = out->validate_bindings(core, writable);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    return {true, std::move(out), {}};
  }
  bool bound_to(const WorldBundle &world) const noexcept override {
    return &world == &world_;
  }
  Status validate_bindings(const NativeCompleteWorldCoreSources &core,
                           bool writable) const override {
    if (&core != &core_ || &core.characters.early.world != &world_ ||
        writable != writable_ || !world_.catalog ||
        bool(world_.catalog->economic_facilities) != bool(installed_) ||
        bool(installed_) != bool(facility_) ||
        exports_.facility != (facility_ ? &*facility_ : nullptr) ||
        exports_.installed_original.get() != installed_.get() ||
        exports_.market_original.get() != market_.get() ||
        bool(world_.catalog->household_purchases) != bool(household_))
      return common_failure("CompleteDomainOriginalOwnerMismatch", {},
                            ErrorKind::internal);
    if (installed_) {
      auto valid = installed_->validate_bindings();
      if (!valid.ok)
        return valid;
      auto now = world_.actual_phase_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      auto same = compare_world_moments(now.value, facility_->current);
      if (!same.ok)
        return {false, {}, std::move(same.error)};
      if (same.value || &facility_->holders != &core.facility_holders ||
          &facility_->applied != &*installed_)
        return common_failure("CompleteDomainOriginalMomentMismatch");
    }
    if (market_) {
      if (!market_->bound_to(world_))
        return common_failure("CompleteMarketOriginalOwnerMismatch");
      auto valid = market_->validate_bindings();
      if (!valid.ok)
        return valid;
    }
    if (bool(world_.catalog->field_group_passage) != bool(field_group_) ||
        exports_.transport_inventory_entries !=
            (field_group_ ? &field_group_->inventory_entries() : nullptr) ||
        exports_.transport_manifest_history !=
            (field_group_ ? &field_group_->manifests() : nullptr) ||
        bool(exports_.military_remaining) != bool(field_group_))
      return common_failure("CompleteFieldGroupOriginalOwnerMismatch");
    if (field_group_) {
      auto valid = field_group_->validate_bindings();
      if (!valid.ok)
        return valid;
      if (!exports_.military_remaining->bound_to(
              world_, core.characters.early.complete))
        return common_failure("CompleteFieldMilitaryRemainingOwnerMismatch");
    }
    if (household_) {
      if (!household_->bound_to(world_) ||
          exports_.household_service != &household_->service() ||
          exports_.household_purchases != &household_->service().household() ||
          exports_.household_retail != &household_->service().retail() ||
          exports_.private_shop_operations !=
              household_->foundation().operations() ||
          exports_.household_transport !=
              (core.characters.early.transport_business
                   ? &household_->foundation().transport()
                   : nullptr) ||
          exports_.household_foundation !=
              (household_->foundation().operations() ? &household_->foundation()
                                                     : nullptr))
        return common_failure("CompleteDomainOriginalOwnerMismatch", {},
                              ErrorKind::internal);
      return household_->validate_bindings();
    }
    if (exports_.household_service || exports_.household_purchases ||
        exports_.household_retail || exports_.private_shop_operations ||
        exports_.household_foundation || exports_.household_transport)
      return common_failure("CompleteDomainUnexpectedHouseholdSource");
    return common_success();
  }
  const NativeCompleteDomainPrerequisiteExports &
  sources() const noexcept override {
    return exports_;
  }
};
// 每次调用按实际M重新组合原S5读口；不拿初始body或旧phase快照跨采用复用。
class HouseholdScope final : public NativeCompleteWorldSourceScope {
  const WorldBundle &world_;
  WorldMoment at_;
  std::unique_ptr<NativeHouseholdWorldScope> household_;
  HouseholdServiceReadBinding read_;
  std::optional<HouseholdServiceWriteBinding> write_;
  NativeCompleteWorldSourceExports exports_;
  HouseholdScope(const WorldBundle &world, WorldBundle *write,
                 const WorldMoment &at,
                 std::unique_ptr<NativeHouseholdWorldScope> household)
      : world_(world), at_(at), household_(std::move(household)),
        read_{household_->service().household(),
              household_->service().market()} {
    exports_.household = &read_;
    if (write) {
      write_.emplace(HouseholdServiceWriteBinding{
          write->common, *write->household_purchases, *write->action_elapsed,
          read_.household, read_.market});
      exports_.household_write = &*write_;
    }
  }

public:
  static Outcome<std::shared_ptr<HouseholdScope>>
  create(const WorldBundle &world, WorldBundle *write,
         const NativeCompleteWorldCoreSources &core,
         const NativeCompleteDomainPrerequisiteOwner &original,
         const WorldMoment &at, const DynamicStepInput *phase) {
    if (&core.characters.early.world != &world || !original.bound_to(world) ||
        (write && write != &world))
      return {false,
              {},
              common_failure("CompleteHouseholdScopeWorldMismatch").error};
    auto now = world.actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto same = compare_world_moments(now.value, at);
    if (!same.ok)
      return {false, {}, std::move(same.error)};
    if (same.value)
      return {false,
              {},
              common_failure("CompleteHouseholdScopeMomentMismatch").error};
    NativeHouseholdWorldLateSources late;
    late.transport_business = core.characters.early.transport_business;
    if (world.catalog->growth_market_history &&
        world.catalog->growth_market_history->selected_version == 2) {
      auto actual = make_market_original(
          core, original.sources().installed_original, phase, nullptr);
      if (!actual.ok)
        return {false, {}, std::move(actual.error)};
      late.actual_market = std::shared_ptr<const NativeEconomicFifoRead>(
          actual.value, &actual.value->fifo());
    }
    auto household =
        NativeHouseholdWorldScope::create(core, std::move(late), phase);
    if (!household.ok)
      return {false, {}, std::move(household.error)};
    return {true,
            checkpoint_share(std::unique_ptr<HouseholdScope>(new HouseholdScope(
                world, write, at, std::move(household.value)))),
            {}};
  }
  bool bound_to(const WorldBundle &world) const noexcept override {
    return &world == &world_;
  }
  NativeCompleteWorldRoute route() const noexcept override {
    return NativeCompleteWorldRoute::household;
  }
  const WorldMoment &actual_at() const noexcept override { return at_; }
  const NativeCompleteWorldSourceExports &sources() const noexcept override {
    return exports_;
  }
};
// 政治写scope借真生产前序，前序先声明，确保写lease先析构。
class RetainedPoliticalWrite final : public NativeCompleteWorldSourceScope {
  std::shared_ptr<const NativeEconomicFifoWorldScope> production_;
  std::shared_ptr<NativePoliticalWorldScope> political_;

public:
  RetainedPoliticalWrite(std::shared_ptr<const NativeEconomicFifoWorldScope> p,
                         std::shared_ptr<NativePoliticalWorldScope> s)
      : production_(std::move(p)), political_(std::move(s)) {}
  bool bound_to(const WorldBundle &w) const noexcept override {
    return production_->bound_to(w) && political_->bound_to(w);
  }
  NativeCompleteWorldRoute route() const noexcept override {
    return NativeCompleteWorldRoute::political;
  }
  const WorldMoment &actual_at() const noexcept override {
    return political_->actual_at();
  }
  const NativeCompleteWorldSourceExports &sources() const noexcept override {
    return political_->sources();
  }
};
} // namespace
Outcome<std::unique_ptr<NativeCompleteDomainPrerequisiteOwner>>
make_native_complete_domain_read_prerequisites(
    const NativeCompleteWorldCoreSources &core) {
  return DomainPrerequisites::create(core, nullptr);
}
Outcome<std::unique_ptr<NativeCompleteDomainPrerequisiteOwner>>
make_native_complete_domain_write_prerequisites(
    const NativeCompleteWorldCoreSources &core, WorldBundle &world,
    CompleteExactWorldBindingOwner &complete) {
  if (&core.characters.early.world != &world ||
      &core.characters.early.complete != &complete)
    return {
        false, {}, common_failure("CompleteDomainMutableWorldMismatch").error};
  return DomainPrerequisites::create(core, &world);
}
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_household_read_scope(
    const WorldBundle &world, const NativeCompleteWorldCoreSources &core,
    const NativeCompleteDomainPrerequisiteOwner &original,
    const WorldMoment &at) {
  auto out =
      HouseholdScope::create(world, nullptr, core, original, at, nullptr);
  return out.ok
             ? Outcome<std::shared_ptr<
                   const NativeCompleteWorldSourceScope>>{true,
                                                          std::move(out.value),
                                                          {}}
             : Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>{
                   false, {}, std::move(out.error)};
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_household_write_scope(
    WorldBundle &world, const NativeCompleteWorldCoreSources &core,
    NativeCompleteDomainPrerequisiteOwner &original,
    CompleteExactWorldBindingOwner &complete, const WorldMoment &at,
    ExactPhaseWriteContext *, const DynamicStepInput *phase) {
  if (&core.characters.early.complete != &complete)
    return {false,
            {},
            common_failure("CompleteHouseholdMutableOwnerMismatch").error};
  auto out = HouseholdScope::create(world, &world, core, original, at, phase);
  return out.ok ? Outcome<std::shared_ptr<
                      NativeCompleteWorldSourceScope>>{true,
                                                       std::move(out.value),
                                                       {}}
                : Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>{
                      false, {}, std::move(out.error)};
}
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_political_read_scope(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    const NativeCompleteDomainPrerequisiteOwner &original,
    const WorldMoment &at) {
  if (!original.bound_to(w) || &core.characters.early.world != &w)
    return {
        false, {}, common_failure("CompletePoliticalScopeWorldMismatch").error};
  NativePoliticalWorldLateRead late;
  const auto &installed = original.sources().installed_original;
  late.installation = installed ? &installed->installation() : nullptr;
  auto out = NativePoliticalWorldScope::create_read(w, core, at, late);
  if (!out.ok)
    return {false, {}, std::move(out.error)};
  return {true, std::move(out.value), {}};
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_political_write_scope(
    WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    NativeCompleteDomainPrerequisiteOwner &original,
    CompleteExactWorldBindingOwner &complete, const WorldMoment &at,
    ExactPhaseWriteContext *, const DynamicStepInput *phase) {
  if (!original.bound_to(w) || &core.characters.early.world != &w ||
      &core.characters.early.complete != &complete)
    return {
        false, {}, common_failure("CompletePoliticalScopeWorldMismatch").error};
  auto actual = make_market_original(
      core, original.sources().installed_original, phase,
      w.production_cycles ? &*w.production_cycles : nullptr);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  auto market = std::move(actual.value);
  if (!market->production_inputs() || !market->production())
    return {false,
            {},
            common_failure("CompletePoliticalProductionOriginalScopeRequired")
                .error};
  NativePoliticalWorldLateRead late;
  const auto &installed = original.sources().installed_original;
  late.installation = installed ? &installed->installation() : nullptr;
  late.actual_phase = phase;
  auto out = NativePoliticalWorldScope::create_write(
      w, core, at, {late, *market->production_inputs(), *market->production()});
  if (!out.ok)
    return {false, {}, std::move(out.error)};
  return {true,
          checkpoint_share(std::unique_ptr<RetainedPoliticalWrite>(
              new RetainedPoliticalWrite(market, std::move(out.value)))),
          {}};
}
} // namespace hanmo::resident
