#pragma once
#include "state_need_period.hpp"
#include <memory>
namespace hanmo::resident {
// Each factory owns the original S3/S4/S10 adapters for one actual
// candidate/phase. It must not retain a WorldBundle or a read stamp across a
// write boundary.
struct StateNeedReadBinding {
  const StateNeedPolicyRead &policies;
  const StateGoalRead &goals;
  const StateTaskRead &tasks;
};
struct StateNeedWriteBinding {
  const StateNeedPolicyRead &policies;
  const StateGoalRead &goals;
  StateTaskConsumer &tasks;
};
class StateNeedReadLease {
public:
  virtual ~StateNeedReadLease() = default;
  virtual StateNeedReadBinding bindings() const = 0;
};
class StateNeedWriteLease {
public:
  virtual ~StateNeedWriteLease() = default;
  virtual StateNeedWriteBinding bindings() = 0;
};
struct StateNeedWorldFactory {
  Outcome<std::shared_ptr<const StateNeedReadLease>> (*read)(
      const WorldBundle &, const WorldMoment &) = nullptr;
  Outcome<std::shared_ptr<StateNeedWriteLease>> (*write)(
      WorldBundle &, const WorldMoment &) = nullptr;
};
StateNeedWorldFactory native_state_need_world_factory() noexcept;
// This single Affairs registration retains the original quantity/supply early
// tail and the StateNeed deadline barrier. Real state-satisfaction remains
// required.
ExactPhaseRegistration
make_state_need_deadline_phase_registration(StateNeedWorldFactory,
                                            Integer selected_version = 2);
// 0 只表示实际目录未选：原政策、状态、命令和事实残留均必须不存在。
Status validate_state_need_catalog_absence(const PreparedCatalog &);
Status validate_state_need_world_absence(const WorldBundle &);
Outcome<Integer> read_state_need_selected_version(const CatalogIdentity &);
Status validate_state_need_selected_version(const StateNeedState &,
                                            const CatalogIdentity &,
                                            bool complete_time2_selected);
} // namespace hanmo::resident
