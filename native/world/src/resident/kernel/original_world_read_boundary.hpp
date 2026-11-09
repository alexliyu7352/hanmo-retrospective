// Actual S0 read-boundary proof, not a clock, second current time, or business
// authority.
#pragma once
#include "read_stamp.hpp"
namespace hanmo::resident {
struct WorldBundle;
class CommonStore;
class OriginalWorldReadBoundary final {
  const WorldBundle &world_;
  const CurrentMomentRead &clock_;

public:
  OriginalWorldReadBoundary(const WorldBundle &world,
                            const CurrentMomentRead &clock)
      : world_(world), clock_(clock) {}
  OriginalWorldReadBoundary(const OriginalWorldReadBoundary &) = delete;
  OriginalWorldReadBoundary &
  operator=(const OriginalWorldReadBoundary &) = delete;
  bool bound_to(const CommonStore &, const CurrentMomentRead &) const noexcept;
  // Stable reads or the actual selected advance endpoint, including a real
  // zero-length due callback at closing before Common publishes the new tick.
  Outcome<WorldMoment> read_observed_moment() const;
  // Original player-command writers cannot borrow a future advance endpoint.
  Outcome<WorldMoment> read_committed_moment() const;
};
} // namespace hanmo::resident
