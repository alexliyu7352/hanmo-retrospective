#include "native_complete_world_registry.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "native_complete_world_sources.hpp"
#include "native_life_age_world.hpp"
#include "resident/domains/family/native_empty_care_phase.hpp"
#include "world_bundle.hpp"
#include "native_private_shop_operations.hpp"
#include "resident/domains/local_economy/private_shop_market_stage.hpp"
#include "resident/domains/commerce/private_shop_capability.hpp"
#include "resident/storage/resource_execution_coverage.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
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
template <class T, class F> Outcome<T> guarded(F &&f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("NativeCompleteConstructionResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("NativeCompleteConstructionResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
template <class T> const T &require(const T *p, const char *code) {
  if (!p)
    missing(code);
  return *p;
}
void scope(const WorldBundle &w, const NativeCompleteWorldSourceScope *s,
           NativeCompleteWorldRoute route, const WorldMoment &at) {
  if (!s || !s->bound_to(w) || s->route() != route)
    missing("CompleteDomainActualSourceScopeRequired");
  if (take(compare_world_moments(s->actual_at(), at)) ||
      take(compare_world_moments(take(w.actual_phase_moment()), at)))
    missing("CompleteDomainActualSourceScopeMomentMismatch");
}
// Plain bindings contain only original same-scope references. They are owned
// by the source scope below, which is in turn retained before the inner lease.
template <class Lease, class Binding>
class BindingRead final : public Lease, public CheckpointAllocatedObject {
  const NativeCompleteWorldSourceScope &scope_;
  Binding binding_;

public:
  BindingRead(const NativeCompleteWorldSourceScope &s, Binding b)
      : scope_(s), binding_(std::move(b)) {
  }
  Binding bindings() const override {
    return binding_;
  }
  bool bound_to(const WorldBundle &w) const noexcept {
    return scope_.bound_to(w);
  }
};
template <class Lease, class Binding>
class BindingWrite final : public Lease, public CheckpointAllocatedObject {
  const NativeCompleteWorldSourceScope &scope_;
  Binding binding_;

public:
  BindingWrite(const NativeCompleteWorldSourceScope &s, Binding b)
      : scope_(s), binding_(std::move(b)) {
  }
  Binding bindings() override {
    return binding_;
  }
  bool bound_to(const WorldBundle &w) const noexcept {
    return scope_.bound_to(w);
  }
};
template <class Lease, class Binding>
class ScopedRead final : public Lease, public CheckpointAllocatedObject {
  std::shared_ptr<const NativeCompleteWorldSourceScope> scope_;
  std::shared_ptr<const Lease> inner_;

public:
  ScopedRead(std::shared_ptr<const NativeCompleteWorldSourceScope> s,
             std::shared_ptr<const Lease> inner)
      : scope_(std::move(s)), inner_(std::move(inner)) {
  }
  Binding bindings() const override {
    return inner_->bindings();
  }
  bool bound_to(const WorldBundle &w) const noexcept {
    if constexpr (std::is_same_v<Lease, CraftFacilityReadLease>)
      return scope_->bound_to(w) && inner_->bound_to(w);
    else
      return scope_->bound_to(w);
  }
};
template <class Lease, class Binding>
class ScopedWrite final : public Lease, public CheckpointAllocatedObject {
  std::shared_ptr<NativeCompleteWorldSourceScope> scope_;
  std::shared_ptr<Lease> inner_;

public:
  ScopedWrite(std::shared_ptr<NativeCompleteWorldSourceScope> s,
              std::shared_ptr<Lease> inner)
      : scope_(std::move(s)), inner_(std::move(inner)) {
  }
  Binding bindings() override {
    return inner_->bindings();
  }
  bool bound_to(const WorldBundle &w) const noexcept {
    if constexpr (std::is_same_v<Lease, CraftFacilityWriteLease>)
      return scope_->bound_to(w) && inner_->bound_to(w);
    else
      return scope_->bound_to(w);
  }
};
class ScopedDirectorRead final : public DirectorExactReadLease,
                                 public CheckpointAllocatedObject {
  std::shared_ptr<const NativeCompleteWorldSourceScope> scope_;
  std::shared_ptr<const DirectorExactReadLease> inner_;

public:
  ScopedDirectorRead(std::shared_ptr<const NativeCompleteWorldSourceScope> s,
                     std::shared_ptr<const DirectorExactReadLease> inner)
      : scope_(std::move(s)), inner_(std::move(inner)) {
  }
  DirectorExactReadBinding reads() const override {
    return inner_->reads();
  }
};
class ScopedDirectorWrite final : public DirectorExactWriteLease,
                                  public CheckpointAllocatedObject {
  std::shared_ptr<NativeCompleteWorldSourceScope> scope_;
  std::shared_ptr<DirectorExactWriteLease> inner_;

public:
  ScopedDirectorWrite(std::shared_ptr<NativeCompleteWorldSourceScope> s,
                      std::shared_ptr<DirectorExactWriteLease> inner)
      : scope_(std::move(s)), inner_(std::move(inner)) {
  }
  DirectorExactWriteBinding writes() override {
    return inner_->writes();
  }
};
template <class Lease, class Binding, class F>
Outcome<std::shared_ptr<const Lease>>
read_scope(const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
           NativeCompleteWorldRoute route, const WorldMoment &at, F &&build) {
  return guarded<std::shared_ptr<const Lease>>([&] {
    auto source = take(graph.read_sources(route, at));
    scope(w, source.get(), route, at);
    std::shared_ptr<const Lease> inner = build(*source);
    if (!inner)
      missing("CompleteDomainReadInnerRequired");
    if constexpr (std::is_same_v<Lease, DirectorExactReadLease>)
      return std::shared_ptr<const Lease>(
          checkpoint_share(std::unique_ptr<ScopedDirectorRead>(
              new ScopedDirectorRead(std::move(source), std::move(inner)))));
    else
      return std::shared_ptr<const Lease>(
          checkpoint_share(std::unique_ptr<ScopedRead<Lease, Binding>>(
              new ScopedRead<Lease, Binding>(std::move(source),
                                             std::move(inner)))));
  });
}
template <class Lease, class Binding, class F>
Outcome<std::shared_ptr<Lease>>
write_scope(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
            NativeCompleteWorldRoute route, const WorldMoment &at,
            ExactPhaseWriteContext *writes, const DynamicStepInput *phase,
            F &&build) {
  return guarded<std::shared_ptr<Lease>>([&] {
    auto source = take(graph.write_sources(route, at, writes, phase));
    scope(w, source.get(), route, at);
    std::shared_ptr<Lease> inner = build(*source);
    if (!inner)
      missing("CompleteDomainWriteInnerRequired");
    if constexpr (std::is_same_v<Lease, DirectorExactWriteLease>)
      return std::shared_ptr<Lease>(
          checkpoint_share(std::unique_ptr<ScopedDirectorWrite>(
              new ScopedDirectorWrite(std::move(source), std::move(inner)))));
    else
      return std::shared_ptr<Lease>(
          checkpoint_share(std::unique_ptr<ScopedWrite<Lease, Binding>>(
              new ScopedWrite<Lease, Binding>(std::move(source),
                                              std::move(inner)))));
  });
}
Outcome<std::shared_ptr<const EconomicExactReadLease>>
economic_read(const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
              const WorldMoment &at) {
  return read_scope<EconomicExactReadLease, EconomicExactReadBinding>(
      w, graph, NativeCompleteWorldRoute::economic, at,
      [&](const auto &source) {
        const auto &out = source.sources();
        if (out.economic_binding) {
          if (out.economic)
            missing("CompleteEconomicDuplicateOriginalConstruction");
          return std::shared_ptr<const EconomicExactReadLease>(checkpoint_share(
              std::unique_ptr<BindingRead<EconomicExactReadLease,
                                          EconomicExactReadBinding>>(
                  new BindingRead<EconomicExactReadLease,
                                  EconomicExactReadBinding>(
                      source, *out.economic_binding))));
        }
        auto lease = std::unique_ptr<NativeEconomicExactReadLease>(
            new NativeEconomicExactReadLease(
                require(source.sources().economic,
                        "CompleteEconomicOriginalConstructionRequired")));
        take(lease->validate_bindings());
        return std::shared_ptr<const EconomicExactReadLease>(
            checkpoint_share(std::move(lease)));
      });
}
Outcome<std::shared_ptr<EconomicExactWriteLease>>
economic_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
               const WorldMoment &at, const DynamicStepInput *phase) {
  return write_scope<EconomicExactWriteLease, EconomicExactWriteBinding>(
      w, graph, NativeCompleteWorldRoute::economic, at, nullptr, phase,
      [&](const auto &source) {
        const auto &out = source.sources();
        if (out.economic_write_binding) {
          if (out.economic_write)
            missing("CompleteEconomicDuplicateOriginalWriteConstruction");
          return std::shared_ptr<EconomicExactWriteLease>(checkpoint_share(
              std::unique_ptr<BindingWrite<EconomicExactWriteLease,
                                           EconomicExactWriteBinding>>(
                  new BindingWrite<EconomicExactWriteLease,
                                   EconomicExactWriteBinding>(
                      source, *out.economic_write_binding))));
        }
        auto dependencies =
            require(source.sources().economic_write,
                    "CompleteEconomicOriginalWriteConstructionRequired");
        dependencies.phase = phase;
        auto lease = std::unique_ptr<NativeEconomicExactWriteLease>(
            new NativeEconomicExactWriteLease(std::move(dependencies)));
        take(lease->validate_bindings());
        return std::shared_ptr<EconomicExactWriteLease>(
            checkpoint_share(std::move(lease)));
      });
}
Outcome<std::shared_ptr<const CraftExactReadLease>>
craft_read(const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
           const WorldMoment &at) {
  return read_scope<CraftExactReadLease, CraftExactReadBinding>(
      w, graph, NativeCompleteWorldRoute::craft, at, [&](const auto &source) {
        auto binding =
            require(source.sources().craft,
                    "CompleteCraftExactOriginalConstructionRequired");
        return std::shared_ptr<const CraftExactReadLease>(checkpoint_share(
            std::unique_ptr<
                BindingRead<CraftExactReadLease, CraftExactReadBinding>>(
                new BindingRead<CraftExactReadLease, CraftExactReadBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<CraftExactWriteLease>>
craft_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
            const WorldMoment &at, ExactPhaseWriteContext &writes) {
  return write_scope<CraftExactWriteLease, CraftExactWriteBinding>(
      w, graph, NativeCompleteWorldRoute::craft, at, &writes, nullptr,
      [&](const auto &source) {
        auto binding =
            require(source.sources().craft_write,
                    "CompleteCraftExactOriginalWriteConstructionRequired");
        return std::shared_ptr<CraftExactWriteLease>(checkpoint_share(
            std::unique_ptr<
                BindingWrite<CraftExactWriteLease, CraftExactWriteBinding>>(
                new BindingWrite<CraftExactWriteLease, CraftExactWriteBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<const RepairExactReadLease>>
repair_read(const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
            const WorldMoment &at) {
  return read_scope<RepairExactReadLease, RepairExactReadBinding>(
      w, graph, NativeCompleteWorldRoute::repair, at, [&](const auto &source) {
        auto binding =
            require(source.sources().repair,
                    "CompleteRepairExactOriginalConstructionRequired");
        return std::shared_ptr<const RepairExactReadLease>(checkpoint_share(
            std::unique_ptr<
                BindingRead<RepairExactReadLease, RepairExactReadBinding>>(
                new BindingRead<RepairExactReadLease, RepairExactReadBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<RepairExactWriteLease>>
repair_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
             const WorldMoment &at, ExactPhaseWriteContext &writes) {
  return write_scope<RepairExactWriteLease, RepairExactWriteBinding>(
      w, graph, NativeCompleteWorldRoute::repair, at, &writes, nullptr,
      [&](const auto &source) {
        auto binding =
            require(source.sources().repair_write,
                    "CompleteRepairExactOriginalWriteConstructionRequired");
        return std::shared_ptr<RepairExactWriteLease>(checkpoint_share(
            std::unique_ptr<
                BindingWrite<RepairExactWriteLease, RepairExactWriteBinding>>(
                new BindingWrite<RepairExactWriteLease,
                                 RepairExactWriteBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<const CraftFacilityReadLease>>
craft_facility_read(const WorldBundle &w,
                    const NativeCompleteWorldSourceOwner &graph,
                    const WorldMoment &at) {
  return read_scope<CraftFacilityReadLease, CraftFacilityReadBinding>(
      w, graph, NativeCompleteWorldRoute::craft_facility, at,
      [&](const auto &source) {
        auto binding =
            require(source.sources().craft_facility,
                    "CompleteCraftFacilityOriginalConstructionRequired");
        return std::shared_ptr<const CraftFacilityReadLease>(checkpoint_share(
            std::unique_ptr<
                BindingRead<CraftFacilityReadLease, CraftFacilityReadBinding>>(
                new BindingRead<CraftFacilityReadLease,
                                CraftFacilityReadBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<CraftFacilityWriteLease>>
craft_facility_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
                     const WorldMoment &at, ExactPhaseWriteContext &writes) {
  return write_scope<CraftFacilityWriteLease, CraftFacilityWriteBinding>(
      w, graph, NativeCompleteWorldRoute::craft_facility, at, &writes, nullptr,
      [&](const auto &source) {
        auto binding =
            require(source.sources().craft_facility_write,
                    "CompleteCraftFacilityOriginalWriteConstructionRequired");
        return std::shared_ptr<CraftFacilityWriteLease>(checkpoint_share(
            std::unique_ptr<BindingWrite<CraftFacilityWriteLease,
                                         CraftFacilityWriteBinding>>(
                new BindingWrite<CraftFacilityWriteLease,
                                 CraftFacilityWriteBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<const HouseholdServiceReadLease>>
household_read(const WorldBundle &w,
               const NativeCompleteWorldSourceOwner &graph,
               const WorldMoment &at) {
  return read_scope<HouseholdServiceReadLease, HouseholdServiceReadBinding>(
      w, graph, NativeCompleteWorldRoute::household, at,
      [&](const auto &source) {
        auto binding =
            require(source.sources().household,
                    "CompleteHouseholdServiceOriginalConstructionRequired");
        return std::shared_ptr<const HouseholdServiceReadLease>(
            checkpoint_share(
                std::unique_ptr<BindingRead<HouseholdServiceReadLease,
                                            HouseholdServiceReadBinding>>(
                    new BindingRead<HouseholdServiceReadLease,
                                    HouseholdServiceReadBinding>(
                        source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<HouseholdServiceWriteLease>>
household_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
                const WorldMoment &at) {
  return write_scope<HouseholdServiceWriteLease, HouseholdServiceWriteBinding>(
      w, graph, NativeCompleteWorldRoute::household, at, nullptr, nullptr,
      [&](const auto &source) {
        auto binding = require(
            source.sources().household_write,
            "CompleteHouseholdServiceOriginalWriteConstructionRequired");
        return std::shared_ptr<HouseholdServiceWriteLease>(checkpoint_share(
            std::unique_ptr<BindingWrite<HouseholdServiceWriteLease,
                                         HouseholdServiceWriteBinding>>(
                new BindingWrite<HouseholdServiceWriteLease,
                                 HouseholdServiceWriteBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<const StateNeedReadLease>>
state_need_read(const WorldBundle &w,
                const NativeCompleteWorldSourceOwner &graph,
                const WorldMoment &at) {
  return read_scope<StateNeedReadLease, StateNeedReadBinding>(
      w, graph, NativeCompleteWorldRoute::state_need, at,
      [&](const auto &source) {
        auto binding = require(source.sources().state_need,
                               "CompleteStateNeedOriginalConstructionRequired");
        return std::shared_ptr<const StateNeedReadLease>(checkpoint_share(
            std::unique_ptr<
                BindingRead<StateNeedReadLease, StateNeedReadBinding>>(
                new BindingRead<StateNeedReadLease, StateNeedReadBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<StateNeedWriteLease>>
state_need_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
                 const WorldMoment &at) {
  return write_scope<StateNeedWriteLease, StateNeedWriteBinding>(
      w, graph, NativeCompleteWorldRoute::state_need, at, nullptr, nullptr,
      [&](const auto &source) {
        auto binding =
            require(source.sources().state_need_write,
                    "CompleteStateNeedOriginalWriteConstructionRequired");
        return std::shared_ptr<StateNeedWriteLease>(checkpoint_share(
            std::unique_ptr<
                BindingWrite<StateNeedWriteLease, StateNeedWriteBinding>>(
                new BindingWrite<StateNeedWriteLease, StateNeedWriteBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<const PoliticalExactReadLease>>
political_read(const WorldBundle &w,
               const NativeCompleteWorldSourceOwner &graph,
               const WorldMoment &at) {
  return read_scope<PoliticalExactReadLease, PoliticalExactReadBinding>(
      w, graph, NativeCompleteWorldRoute::political, at,
      [&](const auto &source) {
        auto binding =
            require(source.sources().political,
                    "CompletePoliticalExactOriginalConstructionRequired");
        return std::shared_ptr<const PoliticalExactReadLease>(checkpoint_share(
            std::unique_ptr<BindingRead<PoliticalExactReadLease,
                                        PoliticalExactReadBinding>>(
                new BindingRead<PoliticalExactReadLease,
                                PoliticalExactReadBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<PoliticalExactWriteLease>>
political_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
                const WorldMoment &at) {
  return write_scope<PoliticalExactWriteLease, PoliticalExactWriteBinding>(
      w, graph, NativeCompleteWorldRoute::political, at, nullptr, nullptr,
      [&](const auto &source) {
        auto binding =
            require(source.sources().political_write,
                    "CompletePoliticalExactOriginalWriteConstructionRequired");
        return std::shared_ptr<PoliticalExactWriteLease>(checkpoint_share(
            std::unique_ptr<BindingWrite<PoliticalExactWriteLease,
                                         PoliticalExactWriteBinding>>(
                new BindingWrite<PoliticalExactWriteLease,
                                 PoliticalExactWriteBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<const MilitaryExactReadLease>>
military_read(const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
              const WorldMoment &at) {
  return read_scope<MilitaryExactReadLease, MilitaryExactReadBinding>(
      w, graph, NativeCompleteWorldRoute::military, at,
      [&](const auto &source) {
        auto binding =
            require(source.sources().military,
                    "CompleteMilitaryExactOriginalConstructionRequired");
        return std::shared_ptr<const MilitaryExactReadLease>(checkpoint_share(
            std::unique_ptr<
                BindingRead<MilitaryExactReadLease, MilitaryExactReadBinding>>(
                new BindingRead<MilitaryExactReadLease,
                                MilitaryExactReadBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<MilitaryExactWriteLease>>
military_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
               const WorldMoment &at, ExactPhaseWriteContext &writes) {
  return write_scope<MilitaryExactWriteLease, MilitaryExactWriteBinding>(
      w, graph, NativeCompleteWorldRoute::military, at, &writes, nullptr,
      [&](const auto &source) {
        auto binding =
            require(source.sources().military_write,
                    "CompleteMilitaryExactOriginalWriteConstructionRequired");
        return std::shared_ptr<MilitaryExactWriteLease>(checkpoint_share(
            std::unique_ptr<BindingWrite<MilitaryExactWriteLease,
                                         MilitaryExactWriteBinding>>(
                new BindingWrite<MilitaryExactWriteLease,
                                 MilitaryExactWriteBinding>(
                    source, std::move(binding)))));
      });
}
Outcome<std::shared_ptr<const MotionExactReadLease>>
motion_read(const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
            const WorldMoment &at) {
  return read_scope<MotionExactReadLease, MotionExactReadBinding>(
      w, graph, NativeCompleteWorldRoute::motion, at, [&](const auto &source) {
        const auto &s = source.sources();
        if (s.motion_personal_read) {
          if (s.motion_group || s.motion_cargo || s.motion_patrol ||
              s.motion_group_write || s.motion_cargo_write || s.motion_personal_write)
            missing("CompletePersonalMotionSourceSelectionMismatch");
          return s.motion_personal_read;
        }
        return take(make_native_motion_read_lease(
            w, s.motion_group, s.motion_cargo, s.motion_patrol));
      });
}
Outcome<std::shared_ptr<MotionExactWriteLease>>
motion_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
             CompleteExactWorldBindingOwner &owner, const WorldMoment &at) {
  return write_scope<MotionExactWriteLease, MotionExactWriteBinding>(
      w, graph, NativeCompleteWorldRoute::motion, at, nullptr, nullptr,
      [&](const auto &source) {
        const auto &s = source.sources();
        if (s.motion_personal_write) {
          if (s.motion_group || s.motion_cargo || s.motion_patrol ||
              s.motion_group_write || s.motion_cargo_write || s.motion_personal_read)
            missing("CompletePersonalMotionSourceSelectionMismatch");
          return s.motion_personal_write;
        }
        return take(make_native_motion_write_lease_in_owner(
            w, owner, s.motion_group_write, s.motion_cargo_write,
            s.motion_patrol));
      });
}
Outcome<std::shared_ptr<const TaskWorkReadLease>> task_work_read(
    const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
    const CompleteExactWorldBindingOwner &owner, const WorldMoment &at) {
  return read_scope<TaskWorkReadLease, TaskWorkReadBinding>(
      w, graph, NativeCompleteWorldRoute::task_work, at,
      [&](const auto &source) {
        return take(make_native_task_work_read_lease_in_owner(
            w, owner,
            require(source.sources().task_work,
                    "CompleteTaskWorkOriginalConstructionRequired"),
            at));
      });
}
Outcome<std::shared_ptr<TaskWorkWriteLease>>
task_work_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
                CompleteExactWorldBindingOwner &owner, const WorldMoment &at) {
  return write_scope<TaskWorkWriteLease, TaskWorkWriteBinding>(
      w, graph, NativeCompleteWorldRoute::task_work, at, nullptr, nullptr,
      [&](const auto &source) {
        return take(make_native_task_work_write_lease_in_owner(
            w, owner,
            require(source.sources().task_work_write,
                    "CompleteTaskWorkOriginalWriteConstructionRequired"),
            at));
      });
}
Outcome<std::shared_ptr<const DirectorExactReadLease>>
director_read(const WorldBundle &w, const NativeCompleteWorldSourceOwner &graph,
              const CompleteExactWorldBindingOwner &owner,
              const WorldMoment &at) {
  return read_scope<DirectorExactReadLease, DirectorExactReadBinding>(
      w, graph, NativeCompleteWorldRoute::director, at,
      [&](const auto &source) {
        return take(make_native_director_read_lease_in_owner(
            w, owner,
            require(source.sources().director,
                    "CompleteDirectorOriginalConstructionRequired"),
            at));
      });
}
Outcome<std::shared_ptr<DirectorExactWriteLease>>
director_write(WorldBundle &w, NativeCompleteWorldSourceOwner &graph,
               CompleteExactWorldBindingOwner &owner, const WorldMoment &at) {
  return write_scope<DirectorExactWriteLease, DirectorExactWriteBinding>(
      w, graph, NativeCompleteWorldRoute::director, at, nullptr, nullptr,
      [&](const auto &source) {
        return take(make_native_director_write_lease_in_owner(
            w, owner,
            require(source.sources().director_write,
                    "CompleteDirectorOriginalWriteConstructionRequired"),
            at));
      });
}
// This compiled adapter only schedules the original two service-period legs.
// It never selects a shop or a phase on the author's behalf.
void private_shop_phase_selection(const PreparedCatalog &catalog) {
  take(validate_native_private_shop_operation_catalog(catalog));
  if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !take(selected_native_private_shop_operation_versions(catalog.identity))
           .exact_operations())
    missing("CompletePrivateShopOriginalSelectionRequired");
  const ExactPhaseSelection *selected = nullptr;
  for (const auto &row : catalog.exact_time->complete->phase_providers)
    if (row.provider == ExactProviderId::commerce) {
      if (selected)
        missing("DuplicateCompleteCommercePhaseSelection");
      selected = &row;
    }
  if (!selected || selected->profile_id != "private_shop_service_periods" ||
      selected->version != 1)
    missing("CompletePrivateShopOriginalPhaseRequired");
}
const NativeHouseholdFoundationLease &private_shop_foundation(
    const WorldBundle &world, const CompleteExactWorldReadBindings &reads) {
  const auto &foundation = require(reads.household_foundation,
                                  "CompletePrivateShopFoundationRequired");
  const auto &source = require(foundation.operations(),
                              "CompletePrivateShopOriginalSourcesRequired");
  if (reads.private_shop_operations != &source ||
      &source.shops.common != &world.common ||
      &source.shops.state != &world.private_shops)
    missing("CompletePrivateShopSameWorldSourcesRequired");
  take(foundation.validate_bindings());
  return foundation;
}
Outcome<ExactPhaseRegistration>
private_shop_phase(const PreparedCatalog &catalog) {
  return guarded<ExactPhaseRegistration>([&] {
    private_shop_phase_selection(catalog);
    ExactPhaseRegistration phase;
    phase.provider = ExactProviderId::commerce;
    phase.profile_id = "private_shop_service_periods";
    phase.version = 1;
    phase.stages = {ExactStage::action_completions};
    phase.effect_writer_keys = {"private_shop_service_periods"};
    phase.validate_author_selection = [](const WorldBundle &world) {
      return guarded<Done>([&] {
        private_shop_phase_selection(*world.catalog);
        take(validate_native_private_shop_operation_layout(world));
        return Done{};
      });
    };
    phase.validate_world_sources = [](const WorldBundle &world,
                                      const WorldMoment &at) {
      return guarded<Done>([&] {
        if (take(compare_world_moments(at, take(world.actual_phase_moment()))))
          missing("CompletePrivateShopPhaseMomentMismatch");
        auto owner = take(world.complete_exact_time_bindings());
        if (!owner)
          missing("CompletePrivateShopOwningLeaseRequired");
        const auto &foundation = private_shop_foundation(world, owner->reads());
        take(foundation.validate_sources());
        take(validate_private_shop_operation_capabilities(*foundation.operations()));
        return Done{};
      });
    };
    phase.predict = [](const WorldBundle &world, const WorldMoment &from,
                        const WorldMoment &until) {
      return guarded<std::vector<ExactEventPrediction>>([&] {
        auto owner = take(world.complete_exact_time_bindings());
        if (!owner)
          missing("CompletePrivateShopOwningLeaseRequired");
        return take(predict_native_private_shop_service_periods(
            private_shop_foundation(world, owner->reads()), from, until));
      });
    };
    phase.apply_due = [](WorldBundle &world, const ExactEventPrediction &event,
                          ExactPhaseWriteContext &writes) {
      return guarded<ExactEventApplied>([&] {
        if (world.active_exact_write_context() != &writes ||
            !world.common.actor_condition_capture_identity())
          missing("CompletePrivateShopActualCaptureRequired");
        std::optional<ResourceExecutionCoverageBoundary> coverage;
        if (!world.common.resource_execution_capture_identity())
          coverage.emplace(take(begin_resource_execution_coverage(world.common)));
        auto owner = take(world.complete_exact_time_bindings());
        if (!owner)
          missing("CompletePrivateShopOwningLeaseRequired");
        const auto &reads = owner->reads();
        NativePrivateShopMarketCommandContext context{
            world, private_shop_foundation(world, reads),
            reads.facility_holder_handlers, writes};
        auto applied = take(apply_native_private_shop_service_period_in_candidate(
            context, event));
        if (world.growth_market_capture.open_scopes ||
            world.growth_market_capture.unbound_business)
          missing("CompletePrivateShopMarketCaptureUnclosed");
        if (coverage)
          take(finish_resource_execution_coverage(world.common, *coverage));
        return applied;
      });
    };
    phase.validate_applied_source = [](const WorldBundle &world,
                                       const ExactEventApplied &applied) {
      return guarded<Done>([&] {
        auto owner = take(world.complete_exact_time_bindings());
        if (!owner)
          missing("CompletePrivateShopOwningLeaseRequired");
        const auto &source = *private_shop_foundation(world, owner->reads()).operations();
        const MomentOwnerSource actual{applied.applied_result, applied.source_schema};
        const WorldMoment *at = nullptr;
        if (actual.source_schema == 1 &&
            actual.source_ref.domain == private_shop_module &&
            actual.source_ref.kind == "private_shop_labor_applied")
          at = &take(read_original_private_shop_labor(source, actual))->occurred_at;
        else if (actual.source_schema == 2 &&
                 actual.source_ref.domain == private_shop_module &&
                 actual.source_ref.kind == "private_shop_operating_completed")
          at = &take(read_original_private_shop_operation_terminal(source, actual))->occurred_at;
        else
          missing("CompletePrivateShopAppliedSourceRequired");
        if (take(compare_world_moments(*at, take(world.actual_phase_moment()))))
          missing("CompletePrivateShopAppliedMomentMismatch");
        return Done{};
      });
    };
    return phase;
  });
}
} // namespace
const NativeCompleteWorldFactoryRegistry &
native_complete_world_factory_registry() noexcept {
  // This finite table is the single production registry. The actual source
  // graph constructors below own the native core. Their remaining original
  // domain construction dependencies still fail by name; a linked outer
  // constructor is not a statement that every selected source is ready.
  static const NativeCompleteWorldFactoryRegistry value = [] {
    NativeCompleteWorldFactoryRegistry r;
    r.commerce_phase = &private_shop_phase;
    r.sources_read = &make_native_complete_world_read_sources;
    r.sources_write = &make_native_complete_world_write_sources;
    r.economic_read = &economic_read;
    r.economic_write = &economic_write;
    r.craft_read = &craft_read;
    r.craft_write = &craft_write;
    r.repair_read = &repair_read;
    r.repair_write = &repair_write;
    r.craft_facility_read = &craft_facility_read;
    r.craft_facility_write = &craft_facility_write;
    r.household_read = &household_read;
    r.household_write = &household_write;
    r.motion_read = &motion_read;
    r.motion_write = &motion_write;
    r.state_need_read = &state_need_read;
    r.state_need_write = &state_need_write;
    r.political_read = &political_read;
    r.political_write = &political_write;
    r.task_work_read = &task_work_read;
    r.task_work_write = &task_work_write;
    r.military_read = &military_read;
    r.military_write = &military_write;
    r.director_read = &director_read;
    r.director_write = &director_write;
    r.family_complete = &make_native_empty_care_family_phase_registration;
    return r;
  }();
  return value;
}
Status validate_native_complete_world_factory_routes(
    const NativeCompleteWorldFactoryRegistry &r) {
  if (!r.economic_read || !r.economic_write)
    return common_failure("CompleteEconomicFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.craft_read || !r.craft_write)
    return common_failure("CompleteCraftFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.repair_read || !r.repair_write)
    return common_failure("CompleteRepairFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.craft_facility_read || !r.craft_facility_write)
    return common_failure("CompleteCraftFacilityFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.household_read || !r.household_write)
    return common_failure("CompleteHouseholdFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.motion_read || !r.motion_write)
    return common_failure("CompleteMotionFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.state_need_read || !r.state_need_write)
    return common_failure("CompleteStateNeedFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.political_read || !r.political_write)
    return common_failure("CompletePoliticalFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.task_work_read || !r.task_work_write)
    return common_failure("CompleteTaskWorkFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.military_read || !r.military_write)
    return common_failure("CompleteMilitaryFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  if (!r.director_read || !r.director_write)
    return common_failure("CompleteDirectorFactoryRouteMissing", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
std::array<NativeCompleteWorldRouteStatus, 19>
read_native_complete_world_route_status() noexcept {
  const auto &r = native_complete_world_factory_registry();
  return {
      {{NativeCompleteWorldRoute::source_graph, "same_world_source_graph",
        bool(r.sources_read), bool(r.sources_write),
        "central/original-domain-owners",
        "selected original prerequisite/domain source constructors"},
       {NativeCompleteWorldRoute::economic, "economic", bool(r.economic_read),
        bool(r.economic_write), "local_economy",
        "original production/payment/household dependency graph"},
       {NativeCompleteWorldRoute::craft, "craft", bool(r.craft_read),
        bool(r.craft_write), "craft",
        "CraftJobOriginalRead and original effect owner"},
       {NativeCompleteWorldRoute::repair, "repair", bool(r.repair_read),
        bool(r.repair_write), "repair",
        "RepairOriginalRead and original effect owner"},
       {NativeCompleteWorldRoute::craft_facility, "craft_facility",
        bool(r.craft_facility_read), bool(r.craft_facility_write),
        "craft/local_economy", "selected specialist facility construction"},
       {NativeCompleteWorldRoute::household, "household",
        bool(r.household_read), bool(r.household_write), "local_economy",
        "same-candidate original Household source graph"},
       {NativeCompleteWorldRoute::motion, "motion", bool(r.motion_read),
        bool(r.motion_write), "travel",
        "selected group/cargo/patrol original dependencies"},
       {NativeCompleteWorldRoute::state_need, "state_need",
        bool(r.state_need_read), bool(r.state_need_write),
        "affairs/family/tasks", "StateGoalRead and StateTaskSourceRead owners"},
       {NativeCompleteWorldRoute::political, "political",
        bool(r.political_read), bool(r.political_write),
        "organization/politics",
        "lifecycle/relation/resource/prospective source owners"},
       {NativeCompleteWorldRoute::task_work, "task_work",
        bool(r.task_work_read), bool(r.task_work_write), "tasks",
        "Work target/lifecycle and same-owner Economic dependencies"},
       {NativeCompleteWorldRoute::military, "military", bool(r.military_read),
        bool(r.military_write), "military",
        "selected original actions/calendar/Duty dependency graph"},
       {NativeCompleteWorldRoute::director, "director", bool(r.director_read),
        bool(r.director_write), "director",
        "same-owner Work/Body and selected institutional source"},
       {NativeCompleteWorldRoute::family_birth_due, "family/birth_due",
        bool(r.family_complete), bool(r.family_complete), "family",
        "same-World deferred birth absence and original Life1 phase"},
       {NativeCompleteWorldRoute::family_guardianship,
        "family/guardianship_monitor", bool(r.family_complete),
        bool(r.family_complete), "family",
        "same-World deferred guardianship absence; selected care rejected"},
       {NativeCompleteWorldRoute::estate_phase, "estate/phase",
        bool(r.estate_phase), bool(r.estate_phase), "estate",
        "original exact phase construction"},
       {NativeCompleteWorldRoute::commerce_phase, "commerce/phase",
        bool(r.commerce_phase), bool(r.commerce_phase), "commerce",
        "original exact phase construction"},
       {NativeCompleteWorldRoute::medicine_phase, "medicine/phase",
        bool(r.medicine_phase), bool(r.medicine_phase), "medicine",
        "original exact phase construction"},
       {NativeCompleteWorldRoute::study_phase, "study/phase",
        bool(r.study_phase), bool(r.study_phase), "study",
        "original exact phase construction"},
       {NativeCompleteWorldRoute::arcana_phase, "arcana/phase",
        bool(r.arcana_phase), bool(r.arcana_phase), "arcana",
        "original exact phase construction"}}};
}
// 补接已选领域的真实阶段；已知工厂缺实现时明确失败，未知入口仍保留原拒绝。
Status append_native_complete_selected_phase_routes(
    const PreparedCatalog &catalog,
    std::vector<ExactPhaseRegistration> &phases) {
  auto result = guarded<Done>([&] {
    if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time))
      missing("CompleteExactPhaseConstructionSelectionRequired");
    const auto &r = native_complete_world_factory_registry();
    for (const auto &selected : catalog.exact_time->complete->phase_providers) {
      if (std::any_of(phases.begin(), phases.end(), [&](const auto &p) {
            return p.provider == selected.provider;
          }))
        continue;
      Outcome<ExactPhaseRegistration> (*factory)(const PreparedCatalog &) =
          nullptr;
      const char *code = "CompleteOriginalSelectedPhaseConstructionRequired";
      switch (selected.provider) {
      case ExactProviderId::estate:
        factory = r.estate_phase;
        code = "CompleteEstateOriginalPhaseConstructionRequired";
        break;
      case ExactProviderId::commerce:
        factory = r.commerce_phase;
        code = "CompleteCommerceOriginalPhaseConstructionRequired";
        break;
      case ExactProviderId::medicine:
        factory = r.medicine_phase;
        code = "CompleteMedicineOriginalPhaseConstructionRequired";
        break;
      case ExactProviderId::study:
        factory = r.study_phase;
        code = "CompleteStudyOriginalPhaseConstructionRequired";
        break;
      case ExactProviderId::arcana:
        factory = r.arcana_phase;
        code = "CompleteArcanaOriginalPhaseConstructionRequired";
        break;
      default:
        missing(code);
      }
      if (!factory)
        construction_not_implemented(code);
      auto registration = take(factory(catalog));
      if (registration.provider != selected.provider ||
          registration.profile_id != selected.profile_id ||
          registration.version != selected.version)
        missing("CompleteOriginalPhaseConstructionIdentityMismatch");
      phases.push_back(std::move(registration));
    }
    return Done{};
  });
  return {result.ok, {}, std::move(result.error)};
}
// 要求真实出生与监护阶段组合；未实现时不插入空处理器，也不更改阶段校验。
Outcome<ExactPhaseRegistration>
make_native_complete_family_phase_registration(LifeAgeExactWorldFactory life) {
  return guarded<ExactPhaseRegistration>([&] {
    const auto factory =
        native_complete_world_factory_registry().family_complete;
    if (!factory)
      construction_not_implemented(
          "CompleteFamilyBirthDueAndGuardianshipConstructionRequired");
    auto r = take(factory(life));
    if (r.provider != ExactProviderId::family ||
        r.profile_id != "original_life_age_calendar" || r.version != 1)
      missing("CompleteFamilyOriginalRegistrationIdentityMismatch");
    for (const auto stage : {ExactStage::life_age, ExactStage::birth_due,
                             ExactStage::guardianship_monitor})
      if (std::count(r.stages.begin(), r.stages.end(), stage) != 1)
        missing("CompleteFamilyOriginalStageRouteMissing");
    if (!r.validate_author_selection || !r.validate_world_sources)
      missing("CompleteFamilyOriginalSourceAdmissionRequired");
    // Original compose validates the actual event/calendar handlers and writer
    // exclusivity. This wrapper never inserts a stage with an empty handler.
    return r;
  });
}
} // namespace hanmo::resident
