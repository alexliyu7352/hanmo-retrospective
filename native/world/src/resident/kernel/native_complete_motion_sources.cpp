#include "resident/domains/military/field_battle/field_motion_boundary.hpp"
#include "native_complete_motion_sources.hpp"
#include "source_owned_fields.hpp"
#include "native_field_group_world.hpp"
#include "native_field_passage.hpp"
#include "native_field_world_scope.hpp"
#include "resident/domains/military/field_battle/field_motion_contact.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/native_land_motion_phase.hpp"
#include "world_bundle.hpp"
#include <type_traits>

namespace hanmo::resident {
namespace {
using namespace movement_detail;
bool unselected_kind(std::string_view kind, bool army = false) {
  if (army &&
      (kind.starts_with("group_") || kind == "carrier_position" ||
       kind == "transport_entry_manifest" || kind == "boundary_crossing"))
    return false;
  return kind.starts_with("group_") || kind.starts_with("cargo_handling") ||
         kind == "transport_registration" || kind == "transport_release" ||
         kind == "transport_unit" || kind == "carrier_position" ||
         kind == "transport_entry_manifest" || kind == "boundary_crossing" ||
         kind == "patrol_checkpoint_arrival" || kind == "patrol_final_motion" ||
         kind == "population_transport_position" ||
         kind == "population_transport_binding" ||
         kind == "population_transport_release";
}
bool unselected_domain(std::string_view domain, bool army = false) {
  if (army && domain == "group_transport")
    return false;
  return domain == "group_transport" || domain == "cargo_handling" ||
         domain == "patrol_checkpoint_sources";
}
template <class> inline constexpr bool source_visitor_required = false;
// 沿原有限拥有字段借读所有运动交叉来源。新类型无字段访问时编译拒绝，
// 不能用空 group 表掩盖残留引用，也不对普通名字作子串业务猜测。
struct UnselectedMotionSources {
  bool army = false;
  template <class... T> void operator()(const std::variant<T...> &v) const {
    std::visit(*this, v);
  }
  template <class T> void operator()(const std::optional<T> &v) const {
    if (v)
      (*this)(*v);
  }
  template <class A, class B> void operator()(const std::pair<A, B> &v) const {
    (*this)(v.first);
    (*this)(v.second);
  }
  void operator()(const std::shared_ptr<const CompatibilityFields> &v) const {
    if (v && v->profile != CompatibilityFields::Profile::strict_original)
      fail("CompleteMotionUnknownCompatibilityProfile");
  }
  void operator()(const std::shared_ptr<const ReadStamp> &v) const {
    if (v)
      fail("CompleteMotionRetainedReadStampNotAllowed");
  }
  template <class T> void operator()(const T &v) const {
    if constexpr (std::is_same_v<T, SourceRef>) {
      if (unselected_kind(v.kind, army) || unselected_domain(v.domain, army))
        fail("CompleteMotionUnselectedOriginalSource", v.id);
    } else if constexpr (std::is_same_v<T, ReservationRef>) {
      if (unselected_kind(v.activity_kind, army) ||
          unselected_domain(v.domain_id, army))
        fail("CompleteMotionUnselectedOriginalSource", v.id);
    } else if constexpr (std::is_same_v<T, GroupMovementExecution>) {
      if (!army || !std::holds_alternative<ArmyCarrierRef>(v.carrier))
        fail("CompleteMotionGroupOriginalSourcesRequired");
      source_owned_fields::visit(v, *this);
    } else if constexpr (std::is_same_v<T, CarriedMovementWriter>) {
      if (!army || !std::holds_alternative<ArmyCarrierRef>(v.carrier_ref))
        fail("CompleteMotionGroupOriginalSourcesRequired");
      source_owned_fields::visit(v, *this);
    } else if constexpr (std::is_same_v<T, StableId> ||
                         std::is_arithmetic_v<T> || std::is_enum_v<T> ||
                         std::is_same_v<T, std::monostate> ||
                         std::is_same_v<T, CanonicalFraction> ||
                         std::is_same_v<T, WorldMoment> ||
                         std::is_same_v<T, WorkUnits> ||
                         std::is_same_v<T, HalfDayDuration>) {
      // 纯标量不携带具名原业务引用。
    } else {
      if constexpr (std::is_same_v<T, ActionElapsedState>) {
        if (unselected_kind(v.execution_source_ref, army) ||
            unselected_kind(v.owner_cost_state_ref, army))
          fail("CompleteMotionUnselectedOriginalSource", v.action_ref);
      } else if constexpr (std::is_same_v<T, ActionElapsedTerminalBinding>) {
        if (unselected_kind(v.action_kind, army))
          fail("CompleteMotionUnselectedOriginalSource", v.action_ref);
      }
      if constexpr (requires { v.rows(); }) {
        for (const auto &row : v.rows())
          (*this)(row);
      } else if constexpr (requires {
                             v.begin();
                             v.end();
                           }) {
        for (const auto &row : v)
          (*this)(row);
      } else if constexpr (requires {
                             source_owned_fields::visit(v, *this);
                           }) {
        source_owned_fields::visit(v, *this);
      } else if constexpr (requires {
                             source_owned_fields::visit_payload(v, *this);
                           }) {
        source_owned_fields::visit_payload(v, *this);
      } else {
        static_assert(source_visitor_required<T>,
                      "CompleteMotionOriginalSourceVisitorRequired");
      }
    }
  }
};
void validate_absent_providers(const WorldBundle &w) {
  const bool army = bool(w.catalog->field_group_passage);
  if (army)
    take(validate_native_field_army_transport_roots(w));
  for (const auto &module : w.catalog->identity.module_versions)
    if ((!army && module.first == "group_transport") ||
        module.first == "cargo_handling" ||
        module.first == "patrol_checkpoint_sources")
      fail("CompleteMotionSelectedOriginalProviderRequired", module.first);
  const auto &common = w.common.state();
  for (const auto &[id, fact] : common.facts.rows())
    if (unselected_kind(fact.kind, army) ||
        unselected_domain(fact.domain, army))
      fail("CompleteMotionUnselectedOriginalSource", id);
  for (const auto &[id, action] : common.actions.rows())
    if (unselected_kind(action.kind, army))
      fail("CompleteMotionUnselectedOriginalSource", id);
  for (const auto &history : common.history)
    if (unselected_kind(history.kind, army))
      fail("CompleteMotionUnselectedOriginalSource", history.result_id);
  for (const auto &[id, result] : common.results.rows())
    if (result && unselected_kind(result->kind, army))
      fail("CompleteMotionUnselectedOriginalSource", id);
  const UnselectedMotionSources scan{army};
  for (const auto &[id, wallet] : common.wallets.rows())
    for (const auto &reservation : wallet.reserved)
      scan(reservation.action_id);
  for (const auto &[id, inventory] : common.inventories.rows())
    for (const auto &lot : inventory.lots)
      scan(lot.reserved_by);
  scan(*w.movement_exact);
  scan(*w.spatial_carriers);
  scan(*w.group_movement_executions);
  scan(*w.transport_registrations);
  scan(*w.economic_transport_expenses);
  scan(*w.action_elapsed);
  if (w.population_transport)
    scan(*w.population_transport);
}
void validate_core(const WorldBundle &w,
                   const NativeCompleteWorldCoreSources &core,
                   const NativeCompleteDomainPrerequisiteOwner &prerequisites,
                   const WorldMoment &at) {
  if (&core.characters.early.world != &w || !prerequisites.bound_to(w))
    fail("CompleteMotionWorldMismatch");
  take(validate_native_land_motion_no_sea_sources(w));
  validate_absent_providers(w);
  if (!same_moment(take(w.actual_phase_moment()), at) ||
      !same_moment(take(core.characters.early.clock.read_current_moment()), at))
    fail("CompleteMotionActualMomentRequired");
  const auto &spatial = core.characters.spatial;
  if (&spatial.movement != &*w.movement_exact ||
      &spatial.carriers != &*w.spatial_carriers ||
      &spatial.registrations != &*w.transport_registrations)
    fail("CompleteMotionActualSpatialRootsRequired");
  const auto body = take(core.characters.family.body().sources());
  if (&body.common != &w.common || &body.state != &*w.actor_foundation ||
      &body.catalog != w.catalog->actor_foundation.get() ||
      body.shared != w.catalog->common.get() || body.growth != &w.growth ||
      body.growth_catalog != w.catalog->growth.get() ||
      body.movement != &*w.movement_exact ||
      body.elapsed != &*w.action_elapsed || body.fixed_spatial != &spatial ||
      !same_moment(body.actual_now, at))
    fail("CompleteMotionActualConditionSourcesRequired");
}
// 本次 acquisition 的真实 Field/S1 owner；个人接触仍委托原 Encounter 算法。
class FieldMotionRead final : public MotionGroupReadDependencies,
                              public MotionContactBusinessRead,
                              public MotionFieldBoundaryRead {
  const WorldBundle &world_;
  std::shared_ptr<NativeFieldGroupWorld> group_;
  NativePersonalMotionContactRead personal_;
  NativeFieldMotionContactRead contacts_;

public:
  FieldMotionRead(const WorldBundle &w,
                  std::shared_ptr<NativeFieldGroupWorld> group)
      : world_(w), group_(std::move(group)),
        personal_(w.common, *w.movement_exact, w.interaction),
        contacts_(*group_->field().exact_contacts(), *w.catalog->field_battle,
                  personal_) {}
  const GroupMovementContext &group_context() const override {
    return group_->group();
  }
  const MotionContactBusinessRead &contact_sources() const override {
    return *this;
  }
  // 原两轨接触和场界接触共享同一 owning lease，不交叉借世界。
  Status validate_actual_contact_installation(
      const ExactMovementContact &contact,
      const MomentOwnerSource &source) const override {
    return contacts_.validate_actual_contact_installation(contact, source);
  }
  Outcome<std::vector<ExactEventPrediction>> predict_field_boundaries(
      const WorldMoment &from, const WorldMoment &to,
      ExactArithmeticBudget b) const override {
    return predict_original_field_boundaries(*group_->field().exact_contacts(),
                                             *world_.catalog->field_battle,
                                             from, to, b);
  }
  Status validate_field_boundary_application(
      const ExactEventApplied &event, ExactArithmeticBudget b) const override {
    return validate_original_field_boundary_application(
        *group_->field().exact_contacts(), *group_->field().diplomacy(),
        *world_.catalog->field_battle, event, b);
  }
};
// capture 只在实际 install 调用时借入；不会把初始空写栈固化进长期 owner。
class FieldMotionWrite final : public MotionGroupWriteDependencies,
                               public MotionContactBusinessSources,
                               public MotionFieldBoundaryWrite {
  WorldBundle &world_;
  const NativeCompleteWorldCoreSources &core_;
  std::shared_ptr<NativeFieldGroupWorld> group_;
  NativePersonalMotionContactSources personal_;

public:
  FieldMotionWrite(WorldBundle &w, const NativeCompleteWorldCoreSources &core,
                   std::shared_ptr<NativeFieldGroupWorld> group)
      : world_(w), core_(core), group_(std::move(group)),
        personal_(w.common, w.travel, *w.movement_exact, w.interaction,
                  *w.catalog->travel, w.services, w.services.player_actor_id(),
                  {},
                  [&w](const StableId &id) { return release_action(w, id); }) {}
  const GroupMovementContext &group_context() const override {
    return group_->group();
  }
  MotionContactBusinessSources &contact_sources() override { return *this; }
  // 场界预测不写；采用时仍借本次真实 M 的 Complete/Group/capture。
  Outcome<std::vector<ExactEventPrediction>> predict_field_boundaries(
      const WorldMoment &from, const WorldMoment &to,
      ExactArithmeticBudget b) const override {
    return predict_original_field_boundaries(*group_->field().exact_contacts(),
                                             *world_.catalog->field_battle,
                                             from, to, b);
  }
  Status validate_field_boundary_application(
      const ExactEventApplied &event, ExactArithmeticBudget b) const override {
    return validate_original_field_boundary_application(
        *group_->field().exact_contacts(), *group_->field().diplomacy(),
        *world_.catalog->field_battle, event, b);
  }
  Outcome<ExactEventApplied> apply_field_boundary(
      MotionExactWriteBinding &motion, const ExactEventPrediction &event,
      ExactPhaseWriteContext &writes, ExactArithmeticBudget b) override {
    return guard<ExactEventApplied>([&] {
      take(group_->validate_bindings());
      if (world_.active_exact_write_context() != &writes ||
          &motion.common != &world_.common ||
          !group_->field().exact_contacts() || !group_->field().diplomacy())
        fail("FieldBoundaryActualWriteCaptureRequired");
      return take(apply_original_field_boundary_in_candidate(
          motion, *world_.field_battle, *group_->field().exact_contacts(),
          *group_->field().diplomacy(), *world_.catalog->field_battle, event,
          writes, world_.checkpoint_copy_memory, b));
    });
  }
  Outcome<MomentOwnerSource>
  install_actual_contact_in_candidate(const ExactMovementContact &contact,
                                      ExactPhaseWriteContext &writes) override {
    return guard<MomentOwnerSource>([&] {
      take(group_->validate_bindings());
      if (world_.active_exact_write_context() != &writes ||
          !group_->field().exact_contacts() || !group_->field().diplomacy() ||
          !world_.political_relations)
        fail("FieldMotionActualWriteCaptureRequired");
      NativeFieldMotionContactSources original(
          {world_.common, *world_.field_battle,
           *group_->field().exact_contacts(), *world_.catalog->field_battle,
           *world_.political_relations, *group_->field().diplomacy(),
           world_.social, world_.organization, group_->field().abilities(),
           world_.services.player_actor_id(),
           core_.characters.early.military_access.battle_seed,
           world_.checkpoint_copy_memory, personal_, writes});
      return take(
          original.install_actual_contact_in_candidate(contact, writes));
    });
  }
  Status validate_actual_contact_installation(
      const ExactMovementContact &contact,
      const MomentOwnerSource &source) const override {
    NativePersonalMotionContactRead personal(
        world_.common, *world_.movement_exact, world_.interaction);
    NativeFieldMotionContactRead original(*group_->field().exact_contacts(),
                                          *world_.catalog->field_battle,
                                          personal);
    return original.validate_actual_contact_installation(contact, source);
  }
};
class MotionScope final : public NativeCompleteWorldSourceScope {
  const WorldBundle &world_;
  WorldMoment at_;
  std::shared_ptr<const MotionExactReadLease> read_;
  NativeCompleteWorldSourceExports exports_;
  MotionScope(const WorldBundle &w, const WorldMoment &at,
              std::shared_ptr<const MotionExactReadLease> read,
              std::shared_ptr<MotionExactWriteLease> write)
      : world_(w), at_(at), read_(std::move(read)) {
    if (write)
      exports_.motion_personal_write = std::move(write);
    else
      exports_.motion_personal_read = read_;
  }

public:
  static Outcome<std::shared_ptr<MotionScope>>
  create(const WorldBundle &w, WorldBundle *candidate,
         const NativeCompleteWorldCoreSources &core,
         const NativeCompleteDomainPrerequisiteOwner &prerequisites,
         CompleteExactWorldBindingOwner *complete, const WorldMoment &at) {
    return guard<std::shared_ptr<MotionScope>>([&] {
      if (candidate && (candidate != &w || !complete ||
                        complete != &core.characters.early.complete))
        fail("CompleteMotionWriteScopeMismatch");
      validate_core(w, core, prerequisites, at);
      // 原个人与明选 Army 都在本次实际 M 构图，绝不重进 World Complete getter。
      std::shared_ptr<NativeFieldGroupWorld> field_group;
      std::shared_ptr<const MotionGroupReadDependencies> group_read;
      std::shared_ptr<MotionGroupWriteDependencies> group_write;
      if (w.catalog->field_group_passage) {
        field_group = checkpoint_share(
            take(NativeFieldGroupWorld::create(w, core, candidate)));
        group_read = std::make_shared<FieldMotionRead>(w, field_group);
        if (candidate)
          group_write =
              std::make_shared<FieldMotionWrite>(*candidate, core, field_group);
      }
      auto read = take(make_native_motion_read_lease(w, std::move(group_read)));
      auto binding = read->bindings();
      if (&binding.common != &w.common || &binding.travel != &w.travel ||
          &binding.movement != &*w.movement_exact ||
          &binding.carriers != &*w.spatial_carriers ||
          &binding.group_executions != &*w.group_movement_executions ||
          &binding.payments != &*w.economic_transport_expenses ||
          &binding.travel_catalog != w.catalog->travel.get() ||
          &binding.foundation != &*w.actor_foundation ||
          &binding.actor_catalog != w.catalog->actor_foundation.get() ||
          binding.elapsed != &*w.action_elapsed)
        fail("CompleteMotionOriginalLeaseWorldMismatch");
      std::shared_ptr<MotionExactWriteLease> write;
      if (candidate)
        write = take(make_native_motion_write_lease_in_owner(
            *candidate, *complete, std::move(group_write)));
      // lease 拥有本次原上下文；被借 World/Complete 仍由中央外层 retaining
      // owner 保活。
      return checkpoint_share(std::unique_ptr<MotionScope>(
          new MotionScope(w, at, std::move(read), std::move(write))));
    });
  }
  bool bound_to(const WorldBundle &w) const noexcept override {
    return &w == &world_;
  }
  NativeCompleteWorldRoute route() const noexcept override {
    return NativeCompleteWorldRoute::motion;
  }
  const WorldMoment &actual_at() const noexcept override { return at_; }
  const NativeCompleteWorldSourceExports &sources() const noexcept override {
    return exports_;
  }
};
} // namespace
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_motion_read_scope(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    const NativeCompleteDomainPrerequisiteOwner &original,
    const WorldMoment &at) {
  auto scope = MotionScope::create(w, nullptr, core, original, nullptr, at);
  if (!scope.ok)
    return {false, {}, std::move(scope.error)};
  return {true, std::move(scope.value), {}};
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_motion_write_scope(
    WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    NativeCompleteDomainPrerequisiteOwner &original,
    CompleteExactWorldBindingOwner &complete, const WorldMoment &at,
    ExactPhaseWriteContext *writes, const DynamicStepInput *phase) {
  // 原 motion writer 在真实阶段回调接入 context，不借其它域的晚 phase 参数。
  if (writes || phase)
    return {
        false, {}, common_failure("CompleteMotionUnexpectedLatePhase").error};
  auto scope = MotionScope::create(w, &w, core, original, &complete, at);
  if (!scope.ok)
    return {false, {}, std::move(scope.error)};
  return {true, std::move(scope.value), {}};
}
} // namespace hanmo::resident
