#include "native_complete_world_sources.hpp"
#include "native_field_passage.hpp"
#include "native_field_world_scope.hpp"
#include "political_takeover_integration.hpp"
#include "economic_supply_policy_integration.hpp"
#include "resident/domains/organization/office_authority.hpp"
#include "resident/domains/politics/takeover/politics_takeover_profile.hpp"
#include "resident/domains/politics/takeover/politics_takeover_absence.hpp"
#include "resident/domains/politics/takeover/politics_takeover_original_access.hpp"
#include "resident/domains/military/native_named_military_custody_world.hpp"
#include "resident/domains/actor_foundation/named_custody_availability_internal.hpp"
#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include "resident/domains/actor_foundation/native_field_supply_condition.hpp"
#include "native_action_time_world.hpp"
#include "native_actor_battle_spatial.hpp"
#include "native_complete_director_sources.hpp"
#include "native_complete_domain_prerequisites.hpp"
#include "native_complete_economic_sources.hpp"
#include "native_complete_facility_holders.hpp"
#include "native_complete_facility_sources.hpp"
#include "native_complete_family_prerequisites.hpp"
#include "native_complete_motion_sources.hpp"
#include "native_complete_time_catalog.hpp"
#include "native_economic_facility_roots.hpp"
#include "native_field_application_read.hpp"
#include "native_character_attribute_absence.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1_state.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "native_growth_permanent_world.hpp"
#include "native_interaction_action_time_bindings.hpp"
#include "native_interaction_catalog.hpp"
#include "native_interaction_world.hpp"
#include "native_item_craft_state.hpp"
#include "native_military_state.hpp"
#include "native_military_training_period.hpp"
#include "native_original_social_world.hpp"
#include "native_private_shop_operations.hpp"
#include "native_transport_catalog.hpp"
#include "original_world_read_boundary.hpp"
#include "resident/domains/affairs/native_state_need_world_scope.hpp"
#include "resident/domains/commerce/private_shop_action_time.hpp"
#include "resident/domains/local_economy/economic_facility_provider.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "resident/domains/local_economy/household_service_lease.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/military/formation_original_sources.hpp"
#include "resident/domains/military/military_action_placement_read.hpp"
#include "resident/domains/military/military_roster_history.hpp"
#include "resident/domains/military/native_military_world_scope.hpp"
#include "resident/domains/military/return_original_sources.hpp"
#include "resident/domains/organization/organization_resource_validation_reads.hpp"
#include "world_bundle.hpp"
#include <new>
#include <stdexcept>
#include <utility>
namespace hanmo::resident {
class NativeCompleteWorldSources;
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
[[noreturn]] void missing(const char *code) {
  throw Failure{
      common_failure(code, {}, ErrorKind::unsupported_capability).error};
}
// 仅诊断尚未实现的构造入口；保留原缺项原因并中止候选，不冒充普通校验拒绝。
[[noreturn]] void construction_not_implemented(const char *capability) {
  auto error =
      common_failure("NOT_IMPLEMENTED", {}, ErrorKind::unsupported_capability)
          .error;
  error.reason = capability;
  throw Failure{std::move(error)};
}
[[noreturn]] void mismatch(const char *code) {
  throw Failure{common_failure(code, {}, ErrorKind::internal).error};
}
template <class T, class F> Outcome<T> guard(F &&fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("CompleteSourceGraphResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("CompleteSourceGraphResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
const StableId *module(const WorldBundle &w, const char *name) {
  const StableId *selected = nullptr;
  for (const auto &row : w.catalog->identity.module_versions)
    if (row.first == name) {
      if (selected)
        mismatch("CompleteSourceGraphDuplicateSelection");
      selected = &row.second;
    }
  return selected;
}
void roots(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) ||
      !w.catalog->complete_exact_time_runtime || !w.catalog->common ||
      !w.catalog->military || !w.catalog->family ||
      !w.catalog->actor_foundation || !w.catalog->growth || !w.world_timeline ||
      !w.action_elapsed || !w.actor_foundation || !w.movement_exact ||
      !w.spatial_carriers || !w.transport_registrations || !w.facility_claims)
    missing("CompleteSourceGraphActualRootsRequired");
  if (bool(w.catalog->production_cycles) != bool(w.production_cycles))
    mismatch("CompleteSourceGraphPeriodRootMismatch");
  const auto *action = module(w, "military_action_time");
  if (action && *action != "1")
    missing("CompleteSourceGraphMilitaryActionVersionRequired");
  if (bool(action) != bool(w.military_action_time))
    mismatch("CompleteSourceGraphMilitaryActionRootMismatch");
  take(validate_native_military_state_layout(w));
  take(validate_native_growth_permanent_layout(w));
  take(w.actual_phase_moment());
}
const NativeCompleteDomainScopeFactories &
domain(const NativeCompleteWorldSourceGraphFactories &f,
       NativeCompleteWorldRoute route) {
  switch (route) {
  case NativeCompleteWorldRoute::economic:
    return f.economic;
  case NativeCompleteWorldRoute::craft:
    return f.craft;
  case NativeCompleteWorldRoute::repair:
    return f.repair;
  case NativeCompleteWorldRoute::craft_facility:
    return f.craft_facility;
  case NativeCompleteWorldRoute::household:
    return f.household;
  case NativeCompleteWorldRoute::motion:
    return f.motion;
  case NativeCompleteWorldRoute::state_need:
    return f.state_need;
  case NativeCompleteWorldRoute::political:
    return f.political;
  case NativeCompleteWorldRoute::task_work:
    return f.task_work;
  case NativeCompleteWorldRoute::military:
    return f.military;
  case NativeCompleteWorldRoute::director:
    return f.director;
  default:
    missing("CompleteSourceGraphInvalidDomainRoute");
  }
}
const char *absent(NativeCompleteWorldRoute route, bool write) {
  switch (route) {
  case NativeCompleteWorldRoute::economic:
    return write ? "CompleteEconomicWriteSourceConstructionRequired"
                 : "CompleteEconomicReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::craft:
    return write ? "CompleteCraftWriteSourceConstructionRequired"
                 : "CompleteCraftReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::repair:
    return write ? "CompleteRepairWriteSourceConstructionRequired"
                 : "CompleteRepairReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::craft_facility:
    return write ? "CompleteCraftFacilityWriteSourceConstructionRequired"
                 : "CompleteCraftFacilityReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::household:
    return write ? "CompleteHouseholdWriteSourceConstructionRequired"
                 : "CompleteHouseholdReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::motion:
    return write ? "CompleteMotionWriteSourceConstructionRequired"
                 : "CompleteMotionReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::state_need:
    return write ? "CompleteStateNeedWriteSourceConstructionRequired"
                 : "CompleteStateNeedReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::political:
    return write ? "CompletePoliticalWriteSourceConstructionRequired"
                 : "CompletePoliticalReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::task_work:
    return write ? "CompleteTaskWorkWriteSourceConstructionRequired"
                 : "CompleteTaskWorkReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::military:
    return write ? "CompleteMilitaryWriteSourceConstructionRequired"
                 : "CompleteMilitaryReadSourceConstructionRequired";
  case NativeCompleteWorldRoute::director:
    return write ? "CompleteDirectorWriteSourceConstructionRequired"
                 : "CompleteDirectorReadSourceConstructionRequired";
  default:
    return "CompleteSourceGraphInvalidDomainRoute";
  }
}
void scope(const WorldBundle &w, NativeCompleteWorldRoute route,
           const WorldMoment &at, const NativeCompleteWorldSourceScope *p) {
  if (!p)
    missing("CompleteDomainOwningSourceScopeRequired");
  if (!p->bound_to(w) || p->route() != route ||
      take(compare_world_moments(p->actual_at(), at)))
    mismatch("CompleteDomainOwningSourceScopeMismatch");
}

NativeMilitaryWorldScopeDependencies
military_dependencies(const WorldBundle &w,
                      const NativeCompleteWorldCoreSources &core,
                      const NativeCompleteDomainPrerequisiteOwner &owner) {
  const auto &early = core.characters.early;
  const auto &spatial = core.characters.spatial;
  if (&early.world != &w || !owner.bound_to(w))
    mismatch("CompleteMilitaryScopeCoreMismatch");
  if (!early.periods || !early.formations || !early.returns ||
      !early.military_placements)
    missing(
        "CompleteMilitaryOriginalPeriodFormationReturnConstructionRequired");
  const auto *social = w.services.actor_social_occupancy_world();
  if (!social)
    missing("CompleteMilitaryOriginalSocialOccupancyConstructionRequired");
  const auto &late = owner.sources();
  if (!late.military_remaining)
    missing("CompleteMilitaryRemainingOriginalConstructionRequired");
  return {early.complete,
          core.characters.family,
          early.military_access,
          *early.periods,
          *early.formations,
          *early.returns,
          *early.military_placements,
          spatial.original,
          social->read(),
          spatial.births,
          late.military_naval_history,
          late.military_remaining,
          {}};
}
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
military_read_scope(const WorldBundle &w,
                    const NativeCompleteWorldCoreSources &core,
                    const NativeCompleteDomainPrerequisiteOwner &originals,
                    const WorldMoment &at) {
  return guard<std::shared_ptr<const NativeCompleteWorldSourceScope>>([&] {
    return take(make_native_military_world_read_scope(
        w, military_dependencies(w, core, originals), at));
  });
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
military_write_scope(WorldBundle &w, const NativeCompleteWorldCoreSources &core,
                     NativeCompleteDomainPrerequisiteOwner &originals,
                     CompleteExactWorldBindingOwner &complete,
                     const WorldMoment &at, ExactPhaseWriteContext *writes,
                     const DynamicStepInput *) {
  return guard<std::shared_ptr<NativeCompleteWorldSourceScope>>([&] {
    if (&complete != &core.characters.early.complete)
      mismatch("CompleteMilitaryMutableCoreMismatch");
    if (!writes || w.active_exact_write_context() != writes)
      missing("CompleteMilitaryActualWriteCaptureRequired");
    return take(make_native_military_world_write_scope(
        w, military_dependencies(w, core, originals), at, *writes));
  });
}

// 只闭合早期空间与晚期家户业务的构造环。它不创造登记权限，也不处理 Task/Army
// 的业务规则；五个读口全部由同 World 的真实 Household3 实现负责。
class CompleteHouseholdTransportLink final
    : public TransportRegistrationBusinessRead {
  const TransportRegistrationBusinessRead *target_ = nullptr;
  friend class ::hanmo::resident::NativeCompleteWorldSources;
  // 只允许中央私有装配栈绑定一次；目标由更晚的 Domain owner 保活。
  void bind(const TransportRegistrationBusinessRead &target) {
    if (target_ || &target == this)
      mismatch("CompleteHouseholdTransportAlreadyBound");
    target_ = &target;
  }
  template <class T> Outcome<T> unbound() const {
    return {false,
            {},
            common_failure("CompleteHouseholdTransportConstructionNotFinished",
                           {}, ErrorKind::unsupported_capability)
                .error};
  }

public:
  CompleteHouseholdTransportLink() = default;
  CompleteHouseholdTransportLink(const CompleteHouseholdTransportLink &) =
      delete;
  CompleteHouseholdTransportLink &
  operator=(const CompleteHouseholdTransportLink &) = delete;
  CompleteHouseholdTransportLink(CompleteHouseholdTransportLink &&) = delete;
  CompleteHouseholdTransportLink &
  operator=(CompleteHouseholdTransportLink &&) = delete;
  // 每次发布/校验必须仍绑定原目标，不接受晚替换。
  bool
  bound_to(const TransportRegistrationBusinessRead *target) const noexcept {
    return target && target_ == target;
  }
  Outcome<TransportRegistrationAuthority>
  read_registration_authority(const SourceRef &source) const override {
    return target_ ? target_->read_registration_authority(source)
                   : unbound<TransportRegistrationAuthority>();
  }
  Status validate_registration_authority(
      const TransportRegistrationAuthority &authority) const override {
    return target_ ? target_->validate_registration_authority(authority)
                   : unbound<Done>();
  }
  Status validate_registered_initiator(
      const TransportRegistrationSource &source, const TransportUnitState &unit,
      const GroupInitiator &initiator, const SubjectRef &subject,
      const WorldMoment &at) const override {
    return target_ ? target_->validate_registered_initiator(
                         source, unit, initiator, subject, at)
                   : unbound<Done>();
  }
  Outcome<std::vector<StableId>>
  read_releasable_labor_allocations(const StableId &selection,
                                    const SourceRef &business) const override {
    return target_
               ? target_->read_releasable_labor_allocations(selection, business)
               : unbound<std::vector<StableId>>();
  }
  Status
  validate_transport_handover(const TransportUnitReleaseSource &source,
                              const TransportUnitState &unit) const override {
    return target_ ? target_->validate_transport_handover(source, unit)
                   : unbound<Done>();
  }
};

// 只打破 Core 与原家户/私店合同的构造顺序；未绑定不能读，绑定后逐口委派。
// 不拥有业务状态或回持父图；析构只释放借用指针，不调用原 owner。
class CompleteOriginalActionLink final : public ActionTimeOriginalSourceRead {
  const ActionTimeOriginalSourceRead *target_ = nullptr;
  const char *unbound_error_;
  const char *duplicate_error_;
  friend class ::hanmo::resident::NativeCompleteWorldSources;
  // 中央私有构造栈只绑定一次真家户合同，不接受替换或自行回调外层工厂。
  void bind(const ActionTimeOriginalSourceRead &target) {
    if (target_)
      mismatch(duplicate_error_);
    target_ = &target;
  }
  // 任何构造期误读都保留明确失败，不将未连接解释为空行动集合。
  template <class T> Outcome<T> unbound() const {
    return {
        false,
        {},
        common_failure(unbound_error_, {}, ErrorKind::unsupported_capability)
            .error};
  }

public:
  // 由同一中央图固定持有，借用地址不可复制或搬动。
  explicit CompleteOriginalActionLink(
      const char *unbound = "CompleteHouseholdActionConstructionNotFinished",
      const char *duplicate = "CompleteHouseholdActionAlreadyBound")
      : unbound_error_(unbound), duplicate_error_(duplicate) {}
  CompleteOriginalActionLink(const CompleteOriginalActionLink &) = delete;
  CompleteOriginalActionLink &
  operator=(const CompleteOriginalActionLink &) = delete;
  CompleteOriginalActionLink(CompleteOriginalActionLink &&) = delete;
  CompleteOriginalActionLink &operator=(CompleteOriginalActionLink &&) = delete;
  // 发布前及每次图校验都必须仍指向同一原合同。
  bool bound_to(const ActionTimeOriginalSourceRead &target) const noexcept {
    return target_ == &target;
  }
  // 参与历史仍从原执行源反读，不以 elapsed 或中央索引代证。
  Status
  visit_actor_action_history(const ActionTimeRegistration &r,
                             const StableId &actor,
                             ActionTimeActorHistorySink &sink) const override {
    return target_ ? target_->visit_actor_action_history(r, actor, sink)
                   : unbound<Done>();
  }
  // 原样枚举原 owner 的活动和终局；未绑定时不返回空成功。
  Status
  enumerate_action_time_owners(const ActionTimeRegistration &r,
                               ActionTimeOwnerIdSink &sink) const override {
    return target_ ? target_->enumerate_action_time_owners(r, sink)
                   : unbound<Done>();
  }
  // 真行动证明只由原家户账产生。
  Outcome<ActionTimeOwnerProof>
  read_action_time_owner(const ActionTimeRegistration &r,
                         const StableId &id) const override {
    return target_ ? target_->read_action_time_owner(r, id)
                   : unbound<ActionTimeOwnerProof>();
  }
  // 时刻按原事件类型及版本读取，不自报已完成。
  Outcome<WorldMoment>
  read_action_time_source(const ElapsedTimeSource &s) const override {
    return target_ ? target_->read_action_time_source(s)
                   : unbound<WorldMoment>();
  }
  // 固定工时保留原历史全校验。
  Status validate_fixed_action_sources(
      const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
      const ActionElapsedState &e,
      const std::vector<FixedServiceTransition> &h) const override {
    return target_ ? target_->validate_fixed_action_sources(r, p, e, h)
                   : unbound<Done>();
  }
  // 候选校验保留真实 elapsed store，不退回另一个稳定快照。
  Status validate_fixed_action_sources_in_candidate(
      const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
      const ActionElapsedState &e, const std::vector<FixedServiceTransition> &h,
      const ActionElapsedStore &candidate) const override {
    return target_ ? target_->validate_fixed_action_sources_in_candidate(
                         r, p, e, h, candidate)
                   : unbound<Done>();
  }
  // 不支持的共享工期仍由原合同拒绝。
  Status validate_shared_action_sources(
      const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
      const SharedWorkElapsedState &e) const override {
    return target_ ? target_->validate_shared_action_sources(r, p, e)
                   : unbound<Done>();
  }
  // 继续与停因只读原业务状态。
  Outcome<RunStop>
  read_action_continuation_stop(const ActionTimeRegistration &r,
                                const ActionTimeOwnerProof &p,
                                const WorldMoment &at) const override {
    return target_ ? target_->read_action_continuation_stop(r, p, at)
                   : unbound<RunStop>();
  }
  // 原样传回剩余量及其语义，不造职业倒计时。
  Outcome<OriginalActionRemaining>
  read_original_action_remaining(const ActionTimeRegistration &r,
                                 const ActionTimeOwnerProof &p,
                                 const WorldMoment &at) const override {
    return target_ ? target_->read_original_action_remaining(r, p, at)
                   : unbound<OriginalActionRemaining>();
  }
  // 授权窗能力不因适配器而新增，沿原拒绝或真实校验。
  Outcome<ExhaustedAuthorizationWindow>
  read_exhausted_authorization_window(const ActionTimeRegistration &r,
                                      const ActionTimeOwnerProof &p,
                                      const WorldMoment &at) const override {
    return target_ ? target_->read_exhausted_authorization_window(r, p, at)
                   : unbound<ExhaustedAuthorizationWindow>();
  }
  // 结果地址必须由原 owner 反读。
  Outcome<StableId>
  read_gate_result_id(const SharedWorkGateResult &g) const override {
    return target_ ? target_->read_gate_result_id(g) : unbound<StableId>();
  }
  // 缺失或不适用的原 gate 不改写为成功。
  Outcome<SharedWorkGateResult>
  read_original_gate_result(const StableId &id) const override {
    return target_ ? target_->read_original_gate_result(id)
                   : unbound<SharedWorkGateResult>();
  }
  // 专业成果验真完整委派，不从参数构造凭证。
  Status
  validate_original_gate_result(const SharedWorkGateResult &g) const override {
    return target_ ? target_->validate_original_gate_result(g)
                   : unbound<Done>();
  }
  // noexcept 读口只暴露原能力；未绑定仍是缺源，不伪造 Life period。
  const LifeWorkPeriodSourceRead *life_work_period_sources(
      const LifeWorkPeriodSourceRef &s) const noexcept override {
    return target_ ? target_->life_work_period_sources(s) : nullptr;
  }
};

// 每个晚绑定原 owner 都逐栏核原登记；不能只按领域名扩大可接受行动。
bool same_original_action_registration(const ActionTimeRegistration &a,
                                       const ActionTimeRegistration &b) {
  return a.action_kind == b.action_kind && a.owner_domain == b.owner_domain &&
         a.profile_id == b.profile_id &&
         a.profile_version == b.profile_version &&
         a.source_schema == b.source_schema && a.mode == b.mode &&
         a.remaining_meaning == b.remaining_meaning &&
         a.shared_gate_kind == b.shared_gate_kind &&
         a.retained_wait_phases == b.retained_wait_phases;
}

// Late domain contexts must borrow a constructed holder base, but the genuine
// holder readers themselves borrow those contexts. This private, one-time link
// breaks only that construction ordering; every operation delegates unchanged.
class CompleteFacilityHolderLink final : public FacilityHolderSourceRead {
  const FacilityHolderSourceRead *target_ = nullptr;
  friend class ::hanmo::resident::NativeCompleteWorldSources;
  void bind(const FacilityHolderSourceRead &target) {
    if (target_)
      mismatch("CompleteFacilityHolderAlreadyBound");
    target_ = &target;
  }
  template <class T> Outcome<T> unbound() const {
    return {false,
            {},
            common_failure("CompleteFacilityHolderConstructionNotFinished", {},
                           ErrorKind::unsupported_capability)
                .error};
  }

public:
  CompleteFacilityHolderLink() = default;
  CompleteFacilityHolderLink(const CompleteFacilityHolderLink &) = delete;
  CompleteFacilityHolderLink &
  operator=(const CompleteFacilityHolderLink &) = delete;
  CompleteFacilityHolderLink(CompleteFacilityHolderLink &&) = delete;
  CompleteFacilityHolderLink &operator=(CompleteFacilityHolderLink &&) = delete;
  bool bound_to(const FacilityHolderSourceRead &target) const noexcept {
    return target_ == &target;
  }
  Outcome<FacilityHolderKind>
  read_facility_holder_kind(const ReservationRef &r) const override {
    return target_ ? target_->read_facility_holder_kind(r)
                   : unbound<FacilityHolderKind>();
  }
  Status validate_facility_holder_selection(
      const FacilityHolderProfileSelection &r) const override {
    return target_ ? target_->validate_facility_holder_selection(r)
                   : unbound<Done>();
  }
  Outcome<std::vector<FacilityClaimDependency>>
  enumerate_holder_claim_dependencies(const FacilitySlotRef &r) const override {
    return target_ ? target_->enumerate_holder_claim_dependencies(r)
                   : unbound<std::vector<FacilityClaimDependency>>();
  }
  Status validate_holder_claim_admission(
      const FacilityClaim2 &c,
      const FacilityClaimMutationReceipt &r) const override {
    return target_ ? target_->validate_holder_claim_admission(c, r)
                   : unbound<Done>();
  }
  Status validate_holder_claim_close(
      const FacilityClaim2 &c, FacilityClaimCloseReason reason,
      const FacilityClaimMutationReceipt &r) const override {
    return target_ ? target_->validate_holder_claim_close(c, reason, r)
                   : unbound<Done>();
  }
  Status validate_holder_mutation_source(
      const FacilitySlotRef &s,
      const FacilityClaimMutationReceipt &r) const override {
    return target_ ? target_->validate_holder_mutation_source(s, r)
                   : unbound<Done>();
  }
  Outcome<bool>
  has_actual_facility_service_started(const FacilityClaim2 &c,
                                      const WorldMoment &at) const override {
    return target_ ? target_->has_actual_facility_service_started(c, at)
                   : unbound<bool>();
  }
};

// 只组合 P1 已有 /2 的原责任、根与实际时钟，不走只支持 /1 的旧 WorldAccess。
// 不调用完整 P1 lease 或 Complete getter，避免 custody→政治→军务的构造回边。
class CompleteCustodyTakeoverSources final : public CheckpointAllocatedObject {
  class Clock final : public OrganizationAuthorityClock {
    const WorldBundle &world_;
  public:
    explicit Clock(const WorldBundle &w) : world_(w) {}
    Outcome<WorldMoment> read_organization_authority_now() const override {
      return world_.actual_phase_moment();
    }
  } clock_;
  std::optional<EstateEmploymentWorldAccess> employment_;
  std::optional<PrivateShopResponsibilitySourceContext> shops_;
  std::optional<EconomicSupplyPendingAccess> supply_;
  std::optional<PoliticalTakeoverOriginalAccess> originals_;
  std::optional<PoliticalTakeoverContext> context_;
public:
  CompleteCustodyTakeoverSources(const WorldBundle &w,
                                const NativeCompleteWorldCoreSources &core)
      : clock_(w) {
    const auto &c = *w.catalog;
    if (&core.characters.early.world != &w || !c.common || !c.organization ||
        !c.politics || !c.local_economy || !c.military || !c.tasks || !c.affairs ||
        !w.organization_runtime || !c.political_takeover ||
        !c.political_takeover->enabled || c.political_takeover->version != 2 ||
        !w.dynamic_enabled() || !w.politics_enabled())
      missing("CompleteCustodyTakeoverOriginalRootsRequired");
    if (take(compare_world_moments(take(clock_.read_organization_authority_now()),
                                   take(core.characters.early.clock.read_current_moment()))))
      mismatch("CompleteCustodyTakeoverOriginalClockMismatch");
    take(validate_political_takeover_layout(true, c.identity.module_versions,
        w.political_takeover_seed_complete ? &w.political_takeover : nullptr,
        w.economic_takeover_seed_complete
          ? std::optional<Integer>{w.economic_takeover_execution.version} : std::nullopt,
        w.tasks.takeover_execution
          ? std::optional<Integer>{w.tasks.takeover_execution->version} : std::nullopt,
        c.political_takeover->task_execution_version, 2));
    take(validate_political_takeover_command_history(w.common, true));
    if (w.estate_employment_enabled()) {
      if (!w.estate_employment_seed_complete || !c.estate || !c.interaction || !c.estate_employment)
        missing("CompleteCustodyTakeoverEmploymentRootsRequired");
      employment_.emplace(w, nullptr);
    }
    if (w.private_shops_enabled()) {
      if (!w.private_shops_seed_complete || !c.private_shops ||
          (w.private_shop_daily_rent_enabled() && !w.private_shop_daily_rent_seed_complete))
        missing("CompleteCustodyTakeoverShopRootsRequired");
      shops_.emplace(PrivateShopResponsibilitySourceContext{
          w.common, w.local_economy, *c.local_economy, w.private_shops,
          *c.private_shops, nullptr, c.private_shop_daily_rent.get(),
          w.private_shop_daily_rent_enabled() ? &w.private_shop_daily_rent : nullptr});
    }
    if ((w.fiscal_settlement_enabled() && !w.fiscal_settlement_seed_complete) ||
        (w.public_relief_enabled() && !w.public_relief_seed_complete) ||
        (w.governance_tasks_enabled() && !w.governance_tasks_seed_complete) ||
        (w.envoy_tasks_enabled() && !w.envoy_tasks_seed_complete) ||
        (w.occupation_fiscal_enabled() && !w.occupation_fiscal_settlement_seed_complete))
      missing("CompleteCustodyTakeoverResponsibilityRootsRequired");
    // 原供给布局仍拒不兼容选择；绝不补空 pending 或改变旧 /1 字节。
    take(validate_economic_supply_layout(c.economic_supply_policies_enabled,
                                         c.identity.module_versions));
    if (!c.economic_supply_policies_enabled)
      take(validate_economic_supply_world(w));
    else {
      if (!c.economic_supply_policies || !w.economic_supply_policies_seed_complete ||
          !w.political_supply_adoptions_seed_complete)
        missing("CompleteCustodyTakeoverSupplyRootsRequired");
      supply_.emplace(EconomicSupplyPendingContext{w.common, *c.common,
          w.local_economy, *c.local_economy, *c.economic_supply_policies,
          w.economic_supply_policies, nullptr});
    }
    originals_.emplace(PoliticalTakeoverOriginalContext{
        w.common, w.military, *c.military, core.characters.early.military_access,
        w.local_economy, *c.local_economy,
        w.fiscal_settlement_enabled() ? &w.fiscal_settlement : nullptr,
        w.fiscal_settlement_enabled() ? c.fiscal_settlement.get() : nullptr,
        w.public_relief_enabled() ? &w.public_relief : nullptr, w.tasks, *c.tasks,
        w.governance_tasks_enabled() ? &w.governance_tasks : nullptr,
        w.envoy_tasks_enabled() ? &w.envoy_tasks : nullptr, nullptr,
        w.occupation_fiscal_enabled() ? &w.occupation_fiscal_settlement : nullptr,
        employment_ ? &employment_->context() : nullptr, shops_ ? &*shops_ : nullptr});
    context_.emplace(PoliticalTakeoverContext{
        w.common, *c.common, w.organization, *c.organization, *c.politics,
        *c.political_takeover, w.political_takeover, w.military, *c.military,
        core.characters.early.military_access, *originals_, nullptr,
        c.economic_supply_policies_enabled ? &w.political_supply_adoptions : nullptr,
        supply_ ? &*supply_ : nullptr, &clock_, &*w.organization_runtime, &w.politics});
  }
  CompleteCustodyTakeoverSources(const CompleteCustodyTakeoverSources &) = delete;
  CompleteCustodyTakeoverSources &operator=(const CompleteCustodyTakeoverSources &) = delete;
  const PoliticalTakeoverContext &context() const noexcept { return *context_; }
};

// 原退出 reader 借本 Complete 的 Field body 快照；每个实际调用先核原 M，
// 不刷新旧 snapshot，也不允许身体新 M 包装暗用旧 Field 源。返回仍为原拥有值/世界行。
class SameMomentFieldExitRead final : public FieldBattleExitPlacementRead {
  const FieldBattleExitPlacementRead &original_;
  const CurrentMomentRead &clock_;
  WorldMoment at_;
  template<class T, class F> Outcome<T> read(F &&fn) const {
    auto actual = clock_.read_current_moment();
    if (!actual.ok) return {false, {}, actual.error};
    auto same = compare_world_moments(actual.value, at_);
    if (!same.ok) return {false, {}, same.error};
    if (same.value) return {false, {}, common_failure("CompleteFieldHistorySnapshotMomentMismatch").error};
    return fn();
  }
public:
  SameMomentFieldExitRead(const FieldBattleExitPlacementRead &source,
                         const CurrentMomentRead &clock, WorldMoment at)
      : original_(source), clock_(clock), at_(std::move(at)) {}
  bool bound_to(const CommonStore &common, const MovementExactState &movement) const noexcept override {
    return original_.bound_to(common, movement);
  }
  Outcome<ActualActorSpatialWriteRead> read_field_exit_placement(
      const MomentOwnerSource &source, const StableId &actor) const override {
    return read<ActualActorSpatialWriteRead>([&] { return original_.read_field_exit_placement(source, actor); });
  }
  Outcome<CheckpointVector<ActualActorSpatialWriteRead>> read_field_exit_placement_history(
      const StableId &actor) const override {
    return read<CheckpointVector<ActualActorSpatialWriteRead>>([&] { return original_.read_field_exit_placement_history(actor); });
  }
  Outcome<FieldActorMemberLossPlacementRead> read_field_member_loss_placement(
      const MomentOwnerSource &source, const StableId &actor) const override {
    return read<FieldActorMemberLossPlacementRead>([&] { return original_.read_field_member_loss_placement(source, actor); });
  }
  Outcome<CheckpointVector<FieldActorMemberLossPlacementRead>> read_field_member_loss_placement_history(
      const StableId &actor) const override {
    return read<CheckpointVector<FieldActorMemberLossPlacementRead>>([&] { return original_.read_field_member_loss_placement_history(actor); });
  }
  Outcome<FieldActorArmyExitStablePlacementRead> read_field_army_exit_stable_placement(
      const MomentOwnerSource &source, const StableId &actor) const override {
    return read<FieldActorArmyExitStablePlacementRead>([&] { return original_.read_field_army_exit_stable_placement(source, actor); });
  }
  Outcome<CheckpointVector<ActualActorSpatialWriteRead>> read_field_army_exit_stable_placement_history(
      const StableId &actor) const override {
    return read<CheckpointVector<ActualActorSpatialWriteRead>>([&] { return original_.read_field_army_exit_stable_placement_history(actor); });
  }
  Outcome<CheckpointVector<FieldActorEngagementRead>> read_field_actor_engagement_history(
      const StableId &actor) const override {
    return read<CheckpointVector<FieldActorEngagementRead>>([&] { return original_.read_field_actor_engagement_history(actor); });
  }
};

} // namespace

// 具名仅供 CharacterFamily 的私有两阶段装配友元使用，不导出未完成 provider。
class NativeCompleteWorldSources final : public NativeCompleteWorldSourceOwner {
  const WorldBundle &world_;
  WorldBundle *writable_;
  const CompleteExactWorldBindingOwner &complete_;
  CompleteExactWorldBindingOwner *writable_complete_;
  const NativeCompleteWorldSourceGraphFactories factories_;
  WorldCurrentMomentRead clock_;
  OriginalWorldReadBoundary original_boundary_;
  std::optional<NativeFieldApplicationRead> field_application_;
  // 与实际 Field application 同一早期 owner；晚于 formations/returns 析构。
  std::optional<MilitaryFieldHistoryRead> field_history_;
  std::optional<NativeCharacterAttributeAbsenceRead> attribute_absence_;
  MilitaryAccess military_access_;
  NativeOrganizationResourceValidationReads organization_resources_;
  WorldMoment initial_at_;
  std::optional<NativeMilitaryTrainingPeriodRead> periods_;
  std::optional<NativeFormationSourceRead> formations_;
  std::optional<NativeReturnSourceRead> returns_;
  std::optional<NativeMilitaryActionPlacementRead> military_placements_;
  // 必须先于 Family/CivilianSpatial，且晚于目标 Domain 析构；析构不回调借目标。
  std::optional<CompleteHouseholdTransportLink> transport_business_;
  std::optional<NativeCompleteWorldEarlySources> early_;
  std::unique_ptr<NativeCompleteFamilyPrerequisiteOwner> family_prerequisites_;
  std::optional<GrowthFixedSpatialSources> spatial_;
  std::unique_ptr<NativeCharacterFamilyWorldRead> family_;
  std::unique_ptr<NativeInteractionWorldRead> interaction_;
  std::unique_ptr<NativeOriginalSocialWorldRead> social_;
  std::unique_ptr<NativeOriginalSocialWorldWrite> social_write_;
  std::optional<NativeWorldGrowthPermanentBindings> growth_write_;
  std::optional<NativeCompleteWorldCharacterSources> characters_;
  std::unique_ptr<NativeCompleteActionPrerequisiteOwner> action_prerequisites_;
  std::shared_ptr<CompleteOriginalActionLink> household_action_link_;
  std::shared_ptr<CompleteOriginalActionLink> shop_action_link_;
  std::shared_ptr<const ActionTimeOriginalSourceRead> action_time_;
  std::vector<FacilityHolderRegistration> holder_handlers_;
  CompleteFacilityHolderLink holder_link_;
  std::optional<NativeCompleteWorldCoreSources> core_;
  // Core 先于这些真实晚源构造；逆析构为 custody→territory→exit→Field→Core。
  std::unique_ptr<NativeFieldWorldScope> history_field_;
  std::optional<SameMomentFieldExitRead> history_field_exits_;
  std::unique_ptr<CompleteCustodyTakeoverSources> custody_territory_;
  std::shared_ptr<NativeNamedMilitaryCustodyWorldRead> named_custody_;
  std::unique_ptr<NativeCompleteDomainPrerequisiteOwner> domains_;
  // 真读口借 domains_ 的固定地址 operation context；先于该 owner 析构。
  std::optional<NativePrivateShopActionTimeRead> shop_action_;
  std::optional<NativeCompleteFacilityHolderRead> holders_;
  std::optional<NativeEconomicFacilitySourceRead> facilities_;
  std::optional<NativeCompleteFacilitySources> facility_sources_;
  std::optional<CompleteExactWorldReadBindings> read_;
  bool ready_ = false;
  NativeCompleteWorldSources(const WorldBundle &w, WorldBundle *writable,
                             const CompleteExactWorldBindingOwner &owner,
                             CompleteExactWorldBindingOwner *writable_owner,
                             NativeCompleteWorldSourceGraphFactories factories)
      : world_(w), writable_(writable), complete_(owner),
        writable_complete_(writable_owner), factories_(factories), clock_(w),
        original_boundary_(w, clock_),
        military_access_(w.services.military_access()),
        organization_resources_(
            make_native_organization_resource_validation_reads(
                world_, military_access_)),
        initial_at_(take(clock_.read_current_moment())) {}
  // 按依赖顺序构造真实来源；仅未接入工厂报未实现，空产物及失配仍走原校验。
  void construct() {
    const auto transport =
        take(selected_native_transport_versions(world_.catalog->identity));
    if (transport.group) {
      if (world_.catalog->field_group_passage) {
        take(validate_native_field_army_transport_roots(world_));
      } else {
        if (!world_.catalog->household_purchases ||
            world_.catalog->selected_household_version != 3)
          construction_not_implemented(
              "CompleteOriginalGroupBusinessOwnerRequired");
        transport_business_.emplace();
      }
    }
    take(validate_field_battle_layout(
        world_.catalog->identity, world_.catalog->field_battle.get(),
        world_.field_battle ? &*world_.field_battle : nullptr));
    if (world_.field_battle) {
      if (writable_) {
        if (!writable_complete_ || military_access_.actor_conditions)
          mismatch("FieldSupplyConditionWriterOwnerMismatch");
        military_access_.actor_conditions = std::shared_ptr<MilitaryActorConditionWrite>(
            take(make_native_field_supply_condition_writer(*writable_, *writable_complete_)));
      }
      field_application_.emplace(world_, clock_, original_boundary_);
      field_history_.emplace(
          MilitaryFieldHistoryRead{*world_.field_battle, *field_application_});
    }
    take(validate_character_injury_sources_v1_layout(
        world_.catalog->identity, world_.character_injury_sources
                                      ? &*world_.character_injury_sources
                                      : nullptr));
    if (take(condition_application_facts_selected(
            world_.catalog->identity,
            ConditionApplicationHistoryOwner::injury_sources)))
      attribute_absence_.emplace(world_);
    // These real original leaves require only already-owned World roots. No
    // Complete getter, full Original, Body or pending result is invoked here.
    if (world_.catalog->production_cycles) {
      periods_.emplace(EconomicCalendarIdentityReadContext{
          world_.common, world_.local_economy, *world_.production_cycles,
          *world_.actor_foundation, *world_.catalog->actor_foundation, clock_});
    }
    if (module(world_, "military_action_time")) {
      // 精确来源模式独立于旧 dynamic 账；原选择要求的真根不可缺或伪造。
      take(validate_selected_military_roster_root(
          world_.common, world_.catalog->identity,
          world_.military_exact ? &*world_.military_exact : nullptr));
      take(validate_selected_military_field_history(
          world_.common, world_.military, world_.catalog->identity,
          field_history_ ? &*field_history_ : nullptr));
      formations_.emplace(
          world_.common, world_.military, *world_.military_action_time,
          *world_.movement_exact, *world_.action_elapsed,
          ExactFormationSourceReadContext{
              military_access_, world_.world_timeline->scale_ref, clock_, {}});
      returns_.emplace(
          world_.common, world_.military, *world_.military_action_time,
          *world_.movement_exact, *world_.action_elapsed,
          ExactReturnSourceReadContext{
              *world_.catalog->military,
              military_access_,
              world_.world_timeline->scale_ref,
              clock_,
              world_.catalog->identity,
              world_.military_exact ? &*world_.military_exact : nullptr,
              *formations_,
              {},
              field_history_ ? &*field_history_ : nullptr});
      military_placements_.emplace(*formations_, *returns_);
    }
    early_.emplace(NativeCompleteWorldEarlySources{
        world_, complete_, clock_, military_access_,
        formations_ ? &*formations_ : nullptr, returns_ ? &*returns_ : nullptr,
        periods_ ? &*periods_ : nullptr,
        military_placements_ ? &*military_placements_ : nullptr,
        &original_boundary_,
        field_application_ ? &*field_application_ : nullptr,
        attribute_absence_ ? &*attribute_absence_ : nullptr,
        transport_business_ ? &*transport_business_ : nullptr});
    if (!factories_.family_prerequisites)
      construction_not_implemented(
          "CompleteSpatialSeaCraftControlSourceConstructionRequired");
    family_prerequisites_ = take(factories_.family_prerequisites(*early_));
    if (!family_prerequisites_)
      missing("CompleteFamilyPrerequisiteOwnerRequired");
    if (!family_prerequisites_->bound_to(world_))
      mismatch("CompleteFamilyPrerequisiteOwnerMismatch");
    take(family_prerequisites_->validate_early_bindings(*early_));
    const auto &prior = family_prerequisites_->sources();
    take(validate_native_actor_battle_spatial_sources(world_,
                                                      prior.battle_spatial));
    spatial_.emplace(GrowthFixedSpatialSources{
        *world_.movement_exact, *world_.spatial_carriers,
        *world_.transport_registrations, prior.spatial_original,
        prior.battle_spatial,
        world_.military_exact ? &world_.military_exact->spatial_handoffs
                              : nullptr,
        prior.birth_placement, early_->military_placements});
    NativeCharacterFamilyDependencies family{
        *spatial_,
        prior.sea,
        prior.external_born,
        prior.craft,
        early_->periods,
        early_->formations,
        early_->returns,
        prior.disciple,
        prior.biological,
        prior.collateral,
        prior.birth_placement,
        prior.birth_results,
        early_->field_application_clock,
        world_.character_injury_sources ? &*world_.character_injury_sources
                                        : nullptr,
        early_->attribute_absence};
    const bool original_birth =
        world_.catalog->family_birth && world_.catalog->family_birth->enabled &&
        world_.catalog->family_birth->selected_version == 4;
    family_ = take(NativeCharacterFamilyWorldRead::create_for_complete_assembly(
        world_, *world_.catalog->complete_exact_time_runtime, family,
        original_birth));
    if (!family_)
      missing("CompleteActualCharacterFamilyOwnerRequired");
    take(family_prerequisites_->bind_family_sources(*family_));
    const auto &bound = family_prerequisites_->sources();
    // 晚绑定只能补关系，不能换掉已经被 body/age 借用的早期权威。
    if (&bound.spatial_original != &spatial_->original ||
        bound.battle_spatial.legacy != spatial_->battle.legacy ||
        bound.battle_spatial.field != spatial_->battle.field ||
        bound.sea != family.sea || bound.external_born != family.born ||
        bound.craft != family.craft ||
        bound.birth_placement != family.birth_placement ||
        bound.birth_results != family.birth_results)
      mismatch("CompleteFamilyEarlySourcesChangedDuringBinding");
    if ((bound.disciple &&
         bound.disciple->people.complete_people != &family_->people()) ||
        (bound.biological &&
         bound.biological->people.complete_people != &family_->people()) ||
        (bound.collateral &&
         bound.collateral->recognition.people.complete_people !=
             &family_->people()))
      mismatch("CompleteFamilyRelationPeopleMismatch");
    family_->bind_relation_sources(
        {bound.disciple, bound.biological, bound.collateral});
    take(family_prerequisites_->validate_bindings(*early_));
    take(family_->validate_bindings());
    // 基础Interaction目录不代表选中专业来源；未选仍查真根，不构造空owner。
    const auto interaction_selected =
        take(selected_native_interaction_versions(world_.catalog->identity));
    const auto interaction_response = take(
        selected_native_interaction_response_version(world_.catalog->identity));
    const bool interaction_owner_selected =
        interaction_selected.exact_sources() || interaction_response ||
        (world_.catalog->director &&
         (world_.catalog->director->personal_appointment_attendance ||
          world_.catalog->director->visit_reception)) ||
        bool(world_.catalog->visit_appointment_attendance);
    if (interaction_owner_selected) {
      interaction_ = take(NativeInteractionWorldRead::create(world_, *family_));
    } else {
      take(validate_native_interaction_layout(*world_.catalog,
                                              world_.interaction));
      take(validate_native_interaction_response_layout(*world_.catalog,
                                                       world_.interaction));
    }
    if (world_.catalog->social && world_.catalog->social->original_relations) {
      social_ =
          take(NativeOriginalSocialWorldRead::create(world_, family_->body()));
      if (writable_)
        social_write_ =
            take(NativeOriginalSocialWorldWrite::create(*writable_, *social_));
    }
    if (writable_ &&
        world_.catalog->growth->complete_capability_application_chain)
      growth_write_.emplace(*writable_, family_->body(),
                            family_->work_practice());
    characters_.emplace(NativeCompleteWorldCharacterSources{
        *early_, *spatial_, *family_, interaction_.get(), social_.get()});
    // 选型只来自实际编译后的运行注册，不由今日Action或本次命令推断。
    // 核心来源与下列原交互读口已经由同一拥有图真实持有。
    auto interaction_owners =
        take(make_native_interaction_action_time_bindings(*characters_));
    if (factories_.action_prerequisites) {
      action_prerequisites_ =
          take(factories_.action_prerequisites(*characters_));
      if (!action_prerequisites_)
        missing("CompleteActionPrerequisiteOwnerRequired");
      if (!action_prerequisites_->bound_to(world_))
        mismatch("CompleteActionPrerequisiteOwnerMismatch");
      take(action_prerequisites_->validate_bindings(*characters_));
    } else {
      // Cargo是Travel原读口的额外真实依赖；显式选中时不能用空来源替代。
      if (module(world_, "cargo_handling"))
        construction_not_implemented(
            "CompleteProfessionalActionSourceConstructionRequired");
      for (const auto &selected :
           world_.catalog->complete_exact_time_runtime->action_times) {
        if (selected.owner_domain == "growth" ||
            selected.owner_domain == "tasks" ||
            selected.owner_domain == "travel")
          continue;
        // 原 Field 子图已经实现 Office/Formation；实际 reader 在同 owner
        // 的 ActionTime assembler 中构造，其他军事/职业注册仍走真实缺源拒绝。
        if (selected.owner_domain == "military" &&
            world_.catalog->field_group_passage &&
            (same_original_action_registration(selected,
                                                exact_office_action_registration()) ||
             same_original_action_registration(selected,
                                                exact_formation_action_registration())))
          continue;
        bool supplied = false;
        for (const auto &binding : interaction_owners)
          if (binding.owner_domain == selected.owner_domain)
            supplied = true;
        if (!supplied && selected.owner_domain == "local_economy" &&
            world_.catalog->household_purchases &&
            (world_.catalog->selected_household_version == 2 ||
             world_.catalog->selected_household_version == 3)) {
          const auto &original =
              world_.catalog->selected_household_version == 3
                  ? household_purchase_action_time_registration_v3()
                  : household_purchase_action_time_registration();
          if (same_original_action_registration(selected, original)) {
            if (!household_action_link_)
              household_action_link_ =
                  checkpoint_make_shared<CompleteOriginalActionLink>();
            supplied = true;
          }
        }
        if (!supplied && selected.owner_domain == "private_shops") {
          const auto shops =
              take(selected_native_private_shop_operation_versions(
                  world_.catalog->identity));
          if (shops.exact_operations() &&
              same_original_action_registration(
                  selected,
                  private_shop_operation_action_time_registration())) {
            if (!shop_action_link_)
              shop_action_link_ =
                  checkpoint_make_shared<CompleteOriginalActionLink>(
                      "CompletePrivateShopActionConstructionNotFinished",
                      "CompletePrivateShopActionAlreadyBound");
            supplied = true;
          }
        }
        if (!supplied)
          construction_not_implemented(
              "CompleteProfessionalActionSourceConstructionRequired");
      }
    }
    // 未选原owner不构造空业务provider；原注册全验仍拒缺项与重复绑定。
    auto action_owners =
        action_prerequisites_
            ? action_prerequisites_->action_time_owners()
            : CheckpointVector<CompiledActionTimeOwnerBinding>{};
    if (household_action_link_)
      action_owners.push_back({"local_economy", household_action_link_});
    if (shop_action_link_)
      action_owners.push_back({"private_shops", shop_action_link_});
    for (auto &binding : interaction_owners)
      action_owners.push_back(std::move(binding));
    action_time_ = take(make_native_action_time_sources_in_owner(
        world_, *world_.catalog->complete_exact_time_runtime,
        std::move(action_owners), complete_,
        action_prerequisites_
            ? action_prerequisites_->cargo_sources()
            : std::shared_ptr<const MotionCargoReadDependencies>{},
        &family_->body()));
    if (!action_time_)
      missing("CompleteActualActionTimeOwnerRequired");
    holder_handlers_ = take(
        read_native_complete_facility_holder_registrations(*world_.catalog));
    core_.emplace(NativeCompleteWorldCoreSources{
        *characters_, *action_time_, holder_link_, organization_resources_});
    if (world_.catalog->field_group_passage)
      take(bind_native_complete_family_original_core(*family_prerequisites_, *core_));
    // 真 Field 出口必须有完整 Body/Core；只保存本 owner 的源，不借临时 scope。
    // 保 original_body 的全部真实选择门（包括 AF6 缺原 absence 的拒绝）。
    if (world_.catalog->field_group_passage) {
      history_field_ = take(NativeFieldWorldScope::create_from_original_core(world_, *core_));
      if (!history_field_->placements())
        missing("CompleteOriginalFieldPlacementRequired");
      history_field_exits_.emplace(*history_field_->placements(), clock_, initial_at_);
      spatial_->field_exits = &*history_field_exits_;
      spatial_->history_owner_at = initial_at_;
    }
    const bool custody_selected = take(selected_named_military_custody(world_.catalog->identity));
    if (custody_selected && world_.political_takeover_enabled())
      custody_territory_.reset(new CompleteCustodyTakeoverSources(world_, *core_));
    NamedMilitaryCustodyContext custody_context{
        world_.common, world_.military, *world_.catalog->military,
        world_.catalog->identity, world_.field_battle ? &*world_.field_battle : nullptr,
        world_.catalog->naval ? &world_.naval : nullptr, early_->formations,
        &clock_, &military_access_, early_->field_application_clock};
    // 保留原精确计算两项额度；资源沿原算术入口每次借当前 job，不在来源图固定旧 job。
    ExactArithmeticBudget custody_budget;
    custody_budget.max_limbs = 4096;
    custody_budget.max_steps = 4096;
    auto actual_custody = take(make_native_named_military_custody_world_read(
        world_, *core_, {custody_context, spatial_->original, spatial_->battle,
          custody_territory_ ? &custody_territory_->context() : nullptr,
          spatial_->military_handoffs, spatial_->births, spatial_->military_actions,
          spatial_->field_exits, custody_budget}));
    if (bool(actual_custody) != custody_selected || military_access_.named_custody_releases)
      mismatch("CompleteNamedCustodyOriginalOwnerMismatch");
    if (actual_custody) {
      named_custody_ = std::shared_ptr<NativeNamedMilitaryCustodyWorldRead>(std::move(actual_custody));
      core_->named_custody = named_custody_.get();
      military_access_.named_custody_releases = named_custody_;
      spatial_->custody_releases = &named_custody_->placements();
      spatial_->history_owner_at = initial_at_;
      // 原 S4 只支持 AF6；AF8 不注入非空 provider，不改变作者选择。
      if (named_custody_availability_selected(*world_.catalog->actor_foundation)) {
        take(family_->bind_original_named_custody(named_custody_->availability()));
        if (history_field_)
          take(history_field_->bind_original_named_custody(named_custody_->availability()));
      }
    }
    if (writable_) {
      if (!factories_.domain_write)
        construction_not_implemented(
            "CompleteMutableFacilityDomainSourceConstructionRequired");
      domains_ = take(
          factories_.domain_write(*core_, *writable_, *writable_complete_));
    } else {
      if (!factories_.domain_read)
        construction_not_implemented(
            "CompleteFacilityDomainSourceConstructionRequired");
      domains_ = take(factories_.domain_read(*core_));
    }
    if (!domains_)
      missing("CompleteDomainPrerequisiteOwnerRequired");
    if (!domains_->bound_to(world_))
      mismatch("CompleteDomainPrerequisiteOwnerMismatch");
    const auto &late = domains_->sources();
    // The link was already constructed before Core and all late contexts. Now
    // the genuine readers can borrow completed contexts and are destroyed
    // first.
    const auto items =
        take(selected_native_item_craft_versions(world_.catalog->identity));
    holders_.emplace(
        *world_.facility_claims, initial_at_,
        world_.craft_jobs ? &*world_.craft_jobs : nullptr,
        items.item_instances == 3
            ? (world_.item_instances ? &*world_.item_instances : nullptr)
            : nullptr,
        world_.household_purchases ? &*world_.household_purchases : nullptr);
    take(
        holders_->bind_sources(late.craft_occupancy, late.household_purchases));
    holder_link_.bind(*holders_);
    take(domains_->validate_bindings(*core_, writable_ != nullptr));
    // 原 Foundation 构造只借地址；holder 及所有原根闭合后才绑定真实业务。
    if (transport_business_) {
      if (!late.household_transport || !late.household_purchases)
        missing("CompleteHouseholdTransportOriginalOwnerRequired");
      transport_business_->bind(*late.household_transport);
    } else if (late.household_transport) {
      mismatch("CompleteUnselectedGroupBusinessOwner");
    }
    // 真 Domain 与 holder 都完成后才接原合同；发布前不暴露构造期占位。
    if (household_action_link_)
      household_action_link_->bind(household_action_contracts());
    if (shop_action_link_) {
      shop_action_.emplace(private_shop_action_context());
      shop_action_link_->bind(*shop_action_);
    }
    take(validate_native_economic_facility_layout(world_));
    if (bool(late.facility) != bool(world_.catalog->economic_facilities))
      missing("CompleteEconomicFacilitySelectedOwnerRequired");
    if (late.facility) {
      const auto &facility = *late.facility;
      if (!world_.economic_facility_capacity_sources ||
          !world_.catalog->production_facility_conditions ||
          !world_.production_facility_conditions ||
          !world_.catalog->local_economy ||
          !world_.catalog->local_economy->production_management ||
          !world_.catalog->estate ||
          bool(world_.catalog->private_shops) !=
              world_.private_shops_seed_complete)
        missing("CompleteEconomicFacilitySelectedRootsRequired");
      if (&facility.common != &world_.common ||
          &facility.catalog != world_.catalog->economic_facilities.get() ||
          &facility.state != &*world_.economic_facility_capacity_sources ||
          &facility.condition_catalog !=
              world_.catalog->production_facility_conditions.get() ||
          &facility.conditions != &*world_.production_facility_conditions ||
          &facility.production_assets !=
              &*world_.catalog->local_economy->production_management ||
          &facility.estate_catalog != world_.catalog->estate.get() ||
          &facility.estates != &world_.estate ||
          facility.shops_catalog != world_.catalog->private_shops.get() ||
          &facility.shops != &world_.private_shops ||
          &facility.claims != &*world_.facility_claims ||
          &facility.holders != &holder_link_ ||
          take(compare_world_moments(facility.current, initial_at_)))
        mismatch("CompleteEconomicFacilityOwningRootsMismatch");
      facilities_.emplace(facility);
    }
    if (late.household_service &&
        (&late.household_service->household() != late.household_purchases ||
         &late.household_service->retail() != late.household_retail ||
         &late.household_service->market().holders != &holder_link_))
      mismatch("CompleteHouseholdFacilityOwningRootsMismatch");
    facility_sources_.emplace(world_, late.household_service,
                              facilities_ ? &*facilities_ : nullptr);
    take(facility_sources_->validate_bindings());
    read_.emplace(CompleteExactWorldReadBindings{
        *world_.catalog->complete_exact_time_runtime,
        *action_time_,
        *world_.facility_claims,
        *facility_sources_,
        holder_link_,
        holder_handlers_,
        *world_.movement_exact,
        native_director_exact_world_factory(),
        family_->body(),
        late.household_purchases,
        late.household_retail,
        late.precise_population,
        native_economic_exact_world_factory(),
        &family_->people(),
        &family_->control(),
        late.transport_inventory_entries,
        late.transport_manifest_history,
        family_->life_age(),
        interaction_.get(),
        early_->formations,
        early_->returns,
        early_->periods,
        social_.get(),
        family_->continuation(),
        late.private_shop_operations,
        family_->original_birth_context(),
        &*core_,
        late.household_foundation});
    ready_ = true;
  }
  // 核同一世界、目录、状态、时钟与完整租约后返回真正家户合同。
  const HouseholdPurchaseContracts &household_action_contracts() const {
    if (!domains_ || !domains_->bound_to(world_) || !world_.catalog ||
        !world_.catalog->household_purchases || !world_.household_purchases ||
        !world_.action_elapsed || !world_.catalog->exact_time)
      mismatch("CompleteHouseholdActionOriginalRootsMismatch");
    const auto *service = domains_->sources().household_service;
    if (!service)
      missing("CompleteHouseholdActionOriginalOwnerRequired");
    const auto &x = service->household();
    if (&x != domains_->sources().household_purchases ||
        &x.common != &world_.common ||
        &x.shared != world_.catalog->common.get() ||
        &x.catalog != world_.catalog->household_purchases.get() ||
        &x.state != &*world_.household_purchases ||
        &x.elapsed != &*world_.action_elapsed ||
        &x.scale != &world_.catalog->exact_time->scale_ref ||
        x.selected_household_version !=
            world_.catalog->selected_household_version ||
        take(compare_world_moments(x.now, initial_at_)))
      mismatch("CompleteHouseholdActionOriginalOwnerMismatch");
    take(service->validate_bindings());
    return service->contracts();
  }
  // 原 Shop2 来源必须由同一 Domain 家户根持有；这里只核借用/时刻，
  // 不在初始化构图中提前重放尚未完成的原业务来源。
  const PrivateShopOperationSourceContext &private_shop_action_context() const {
    if (!domains_ || !domains_->bound_to(world_) || !world_.catalog ||
        !world_.catalog->exact_time || !world_.catalog->private_shops ||
        !world_.catalog->household_purchases || !world_.household_purchases ||
        !world_.household_retail_settlements || !world_.action_elapsed ||
        !world_.private_shop_operation_sources ||
        !world_.private_shop_operation_capabilities)
      missing("CompletePrivateShopActionOriginalRootsRequired");
    take(validate_native_private_shop_operation_layout(world_));
    const auto selected = take(selected_native_private_shop_operation_versions(
        world_.catalog->identity));
    if (!selected.exact_operations())
      missing("CompletePrivateShopActionOriginalSelectionRequired");
    const auto &late = domains_->sources();
    const auto *foundation = late.household_foundation;
    if (!foundation || !foundation->operations() ||
        foundation->operations() != late.private_shop_operations ||
        &foundation->service() != late.household_service ||
        !foundation->bound_to(world_.common, world_.local_economy,
                              *world_.household_purchases, world_.private_shops,
                              *world_.household_retail_settlements,
                              *world_.action_elapsed))
      mismatch("CompletePrivateShopActionOriginalOwnerMismatch");
    const auto &x = *foundation->operations();
    if (&x.shops.common != &world_.common ||
        &x.shops.state != &world_.private_shops ||
        &x.shops.catalog != world_.catalog->private_shops.get() ||
        &x.markets != world_.catalog->household_purchases.get() ||
        &x.sources != &*world_.private_shop_operation_sources ||
        &x.capabilities != &*world_.private_shop_operation_capabilities ||
        &x.elapsed != &*world_.action_elapsed ||
        &x.scale != &world_.catalog->exact_time->scale_ref ||
        take(compare_world_moments(take(x.clock.read_current_moment()),
                                   initial_at_)) ||
        take(compare_world_moments(foundation->service().household().now,
                                   initial_at_)) ||
        take(compare_world_moments(foundation->service().market().now,
                                   initial_at_)))
      mismatch("CompletePrivateShopActionOriginalSnapshotMismatch");
    take(foundation->validate_bindings());
    return x;
  }
  void actual(const WorldMoment &at) const {
    if (!ready_ || !core_ || !domains_)
      mismatch("CompleteSourceGraphNotPublished");
    if (take(compare_world_moments(at, take(clock_.read_current_moment()))))
      mismatch("CompleteSourceGraphAcquisitionMomentMismatch");
    // 此图含原body和私铺精确来源快照；跨M必须重建真实owner，不能只刷新出口包装。
    if (take(compare_world_moments(at, initial_at_)))
      mismatch("CompleteSourceGraphSnapshotMomentMismatch");
  }

public:
  // 此 shared 仅由同租约的 MilitaryAccess 借用；先解除内部副本再按成员逆序析构。
  ~NativeCompleteWorldSources() override { military_access_.named_custody_releases.reset(); }
  NativeCompleteWorldSources(const NativeCompleteWorldSources &) = delete;
  NativeCompleteWorldSources &
  operator=(const NativeCompleteWorldSources &) = delete;
  NativeCompleteWorldSources(NativeCompleteWorldSources &&) = delete;
  NativeCompleteWorldSources &operator=(NativeCompleteWorldSources &&) = delete;
  static Outcome<std::unique_ptr<NativeCompleteWorldSourceOwner>>
  create(const WorldBundle &w, WorldBundle *mutable_world,
         const CompleteExactWorldBindingOwner &owner,
         CompleteExactWorldBindingOwner *mutable_owner,
         const NativeCompleteWorldSourceGraphFactories &factories) {
    return guard<std::unique_ptr<NativeCompleteWorldSourceOwner>>([&] {
      roots(w);
      if (bool(mutable_world) != bool(mutable_owner) ||
          (mutable_world && mutable_world != &w) ||
          (mutable_owner && mutable_owner != &owner))
        mismatch("CompleteSourceGraphMutableOwnerMismatch");
      auto out = std::unique_ptr<NativeCompleteWorldSources>(
          new NativeCompleteWorldSources(w, mutable_world, owner, mutable_owner,
                                         factories));
      out->construct();
      take(out->validate_bindings(w, mutable_world != nullptr));
      return std::unique_ptr<NativeCompleteWorldSourceOwner>(std::move(out));
    });
  }
  bool bound_to(const WorldBundle &w) const noexcept override {
    return &world_ == &w;
  }
  Status validate_bindings(const WorldBundle &w, bool write) const override {
    return guard<Done>([&] {
      if (!ready_ || &w != &world_ || write != bool(writable_) || !read_ ||
          !core_ || !family_ || !domains_ || !facility_sources_)
        mismatch("CompleteSourceGraphBindingMismatch");
      roots(w);
      actual(take(clock_.read_current_moment()));
      if (!early_ || &early_->world != &world_ ||
          &early_->military_access != &military_access_ ||
          &core_->organization_resources != &organization_resources_ ||
          !organization_resources_.resource_history ||
          !organization_resources_.prospective_grants)
        mismatch("CompleteOrganizationResourceBindingMismatch");
      if (core_->named_custody != named_custody_.get() ||
          spatial_->custody_releases != (named_custody_ ? &named_custody_->placements() : nullptr) ||
          spatial_->field_exits != (history_field_exits_ ? &*history_field_exits_ : nullptr) ||
          military_access_.named_custody_releases.get() != named_custody_.get())
        mismatch("CompleteNamedCustodyExportMismatch");
      take(validate_growth_fixed_spatial_source_moment(*spatial_, initial_at_));
      const auto body = take(family_->body().sources());
      if (named_custody_availability_selected(body.catalog))
        take(validate_named_custody_availability_bindings(body));
      else if (body.named_custody_availability)
        mismatch("CompleteUnselectedNamedCustodyBodySource");
      take(family_->validate_bindings());
      if (interaction_)
        take(interaction_->validate_bindings());
      if (social_)
        take(social_->validate_bindings());
      if (social_write_)
        take(social_write_->validate_bindings());
      take(family_prerequisites_->validate_bindings(*early_));
      if (action_prerequisites_)
        take(action_prerequisites_->validate_bindings(*characters_));
      take(domains_->validate_bindings(*core_, write));
      if (early_->transport_business !=
              (transport_business_ ? &*transport_business_ : nullptr) ||
          (transport_business_ &&
           !transport_business_->bound_to(
               domains_->sources().household_transport)) ||
          (!transport_business_ && domains_->sources().household_transport))
        mismatch("CompleteHouseholdTransportBindingMismatch");
      if (household_action_link_ &&
          !household_action_link_->bound_to(household_action_contracts()))
        mismatch("CompleteHouseholdActionBindingMismatch");
      if (shop_action_link_) {
        (void)private_shop_action_context();
        if (!shop_action_ || !shop_action_link_->bound_to(*shop_action_))
          mismatch("CompletePrivateShopActionBindingMismatch");
      }
      take(facility_sources_->validate_bindings());
      if (read_->household_foundation !=
              domains_->sources().household_foundation ||
          read_->private_shop_operations !=
              domains_->sources().private_shop_operations ||
          read_->original_core != &*core_ ||
          &read_->character_conditions != &family_->body() ||
          &read_->action_sources != action_time_.get() ||
          &read_->facility_holders != &holder_link_ ||
          !holder_link_.bound_to(*holders_) ||
          &read_->facility_sources != &*facility_sources_ ||
          read_->military_formations != early_->formations ||
          read_->military_returns != early_->returns ||
          read_->military_supply_periods != early_->periods)
        mismatch("CompleteSourceGraphExportMismatch");
      return Done{};
    });
  }
  const CompleteExactWorldReadBindings &reads() const override {
    if (!ready_ || !read_)
      mismatch("CompleteSourceGraphNotPublished");
    actual(take(clock_.read_current_moment()));
    return *read_;
  }
  // 为当前时刻获取原领域读作用域；未接入的构造器明确失败，不伪造可用性。
  Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
  read_sources(NativeCompleteWorldRoute route,
               const WorldMoment &at) const override {
    return guard<std::shared_ptr<const NativeCompleteWorldSourceScope>>([&] {
      actual(at);
      const auto factory = domain(factories_, route).read;
      if (!factory)
        construction_not_implemented(absent(route, false));
      auto out = take(factory(world_, *core_, *domains_, at));
      scope(world_, route, at, out.get());
      return out;
    });
  }
  // 为同一候选获取原领域写作用域；保留可写身份与捕获校验后诊断未接入工厂。
  Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
  write_sources(NativeCompleteWorldRoute route, const WorldMoment &at,
                ExactPhaseWriteContext *writes,
                const DynamicStepInput *input) override {
    return guard<std::shared_ptr<NativeCompleteWorldSourceScope>>([&] {
      actual(at);
      if (!writable_ || !writable_complete_)
        missing("CompleteSourceGraphMutableConstructionRequired");
      if (writes && world_.active_exact_write_context() != writes)
        mismatch("CompleteSourceGraphCaptureMismatch");
      const auto factory = domain(factories_, route).write;
      if (!factory)
        construction_not_implemented(absent(route, true));
      auto out = take(factory(*writable_, *core_, *domains_,
                              *writable_complete_, at, writes, input));
      scope(world_, route, at, out.get());
      return out;
    });
  }
  NativeCompleteWorldWriterExports writers() noexcept override {
    NativeCompleteWorldWriterExports out;
    if (!ready_ || !writable_ || !domains_)
      return out;
    if (growth_write_) {
      out.growth_capability = &*growth_write_;
      out.work_completed_growth = &*growth_write_;
    }
    if (social_write_) {
      out.social_growth = &social_write_->growth_service();
      out.social_task_report = &social_write_->task_report();
      out.social_calendar = &social_write_->calendar();
    }
    out.care_life = domains_->sources().care_life;
    out.growth_start = domains_->sources().growth_start;
    return out;
  }
};
namespace {
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
state_need_read_sources(const WorldBundle &w,
                        const NativeCompleteWorldCoreSources &core,
                        const NativeCompleteDomainPrerequisiteOwner &late,
                        const WorldMoment &at) {
  return guard<std::shared_ptr<const NativeCompleteWorldSourceScope>>([&] {
    if (!late.bound_to(w))
      mismatch("CompleteStateNeedDomainOwnerMismatch");
    return take(make_native_state_need_world_read_scope(
        w, core, late.sources().state_need_original, at));
  });
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
state_need_write_sources(WorldBundle &w,
                         const NativeCompleteWorldCoreSources &core,
                         NativeCompleteDomainPrerequisiteOwner &late,
                         CompleteExactWorldBindingOwner &complete,
                         const WorldMoment &at, ExactPhaseWriteContext *,
                         const DynamicStepInput *) {
  return guard<std::shared_ptr<NativeCompleteWorldSourceScope>>([&] {
    if (&complete != &core.characters.early.complete || !late.bound_to(w))
      mismatch("CompleteStateNeedMutableOwnerMismatch");
    return take(make_native_state_need_world_write_scope(
        w, core, late.sources().state_need_original, at));
  });
}

} // namespace
const NativeCompleteWorldSourceGraphFactories &
native_complete_world_source_graph_factories() noexcept {
  // These slots name the still-missing original domain constructions, not
  // substitute business providers. Native core construction and every routing
  // branch above are real; a linked outer route does not make this table ready.
  static const NativeCompleteWorldSourceGraphFactories value = [] {
    NativeCompleteWorldSourceGraphFactories out;
    out.family_prerequisites = &make_native_complete_family_prerequisites;
    out.domain_read = &make_native_complete_domain_read_prerequisites;
    out.domain_write = &make_native_complete_domain_write_prerequisites;
    out.economic = {&make_native_complete_economic_read_scope,
                    &make_native_complete_economic_write_scope};
    out.director = {&make_native_complete_director_read_scope,
                    &make_native_complete_director_write_scope};
    out.political = {&make_native_complete_political_read_scope,
                     &make_native_complete_political_write_scope};
    out.household = {&make_native_complete_household_read_scope,
                     &make_native_complete_household_write_scope};
    out.motion = {&make_native_complete_motion_read_scope,
                  &make_native_complete_motion_write_scope};
    out.military = {&military_read_scope, &military_write_scope};
    out.state_need = {&state_need_read_sources, &state_need_write_sources};
    return out;
  }();
  return value;
}
Outcome<std::unique_ptr<NativeCompleteWorldSourceOwner>>
make_native_complete_world_read_sources(
    const WorldBundle &w, const CompleteExactWorldBindingOwner &owner) {
  return NativeCompleteWorldSources::create(
      w, nullptr, owner, nullptr,
      native_complete_world_source_graph_factories());
}
Outcome<std::unique_ptr<NativeCompleteWorldSourceOwner>>
make_native_complete_world_write_sources(
    WorldBundle &w, CompleteExactWorldBindingOwner &owner) {
  return NativeCompleteWorldSources::create(
      w, &w, owner, &owner, native_complete_world_source_graph_factories());
}
std::array<NativeCompleteSourceConstructionStatus, 14>
read_native_complete_source_construction_status() noexcept {
  const auto &f = native_complete_world_source_graph_factories();
  return {
      {{"family_prerequisites", bool(f.family_prerequisites),
        bool(f.family_prerequisites), "spatial/sea/craft/control"},
       {"professional_action_sources", bool(f.action_prerequisites),
        bool(f.action_prerequisites), "original-action-owners"},
       {"facility_domain_sources", bool(f.domain_read), bool(f.domain_write),
        "original-facility/household/transport"},
       {"economic", bool(f.economic.read), bool(f.economic.write),
        "local_economy"},
       {"craft", bool(f.craft.read), bool(f.craft.write), "craft"},
       {"repair", bool(f.repair.read), bool(f.repair.write), "craft"},
       {"craft_facility", bool(f.craft_facility.read),
        bool(f.craft_facility.write), "craft/local_economy"},
       {"household", bool(f.household.read), bool(f.household.write),
        "local_economy"},
       {"motion", bool(f.motion.read), bool(f.motion.write), "travel"},
       {"state_need", bool(f.state_need.read), bool(f.state_need.write),
        "affairs/family/tasks"},
       {"political", bool(f.political.read), bool(f.political.write),
        "politics/organization"},
       {"task_work", bool(f.task_work.read), bool(f.task_work.write), "tasks"},
       {"military", bool(f.military.read), bool(f.military.write), "military"},
       {"director", bool(f.director.read), bool(f.director.write),
        "director"}}};
}
} // namespace hanmo::resident
