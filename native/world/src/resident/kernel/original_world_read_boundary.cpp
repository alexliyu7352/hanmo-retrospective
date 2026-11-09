#include "original_world_read_boundary.hpp"
#include "exact_phase_graph.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void require(bool value, const char *code) {
  if (!value)
    throw Failure{common_failure(code, "original_world_read_boundary").error};
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
template <class F> Outcome<WorldMoment> guard(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("OriginalWorldBoundaryResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
} // namespace
bool OriginalWorldReadBoundary::bound_to(
    const CommonStore &common, const CurrentMomentRead &clock) const noexcept {
  return &common == &world_.common && &clock == &clock_;
}
Outcome<WorldMoment> OriginalWorldReadBoundary::read_observed_moment() const {
  return guard([&] {
    require(bool(world_.world_timeline),
            "OriginalWorldBoundaryTimelineRequired");
    auto committed =
        take(read_world_moment(world_.common.tick(), *world_.world_timeline));
    auto actual = take(world_.actual_phase_moment());
    auto clock = take(clock_.read_current_moment());
    require(compare(actual, clock) == 0, "OriginalWorldBoundaryClockMismatch");
    const auto *writes = world_.active_exact_write_context();
    const auto *span = writes ? writes->actual_advance_span() : nullptr;
    const auto &phase = world_.active_read_phase_;
    if (phase)
      require(compare(phase->actual_moment, actual) == 0,
              "OriginalWorldBoundaryPhaseMismatch");
    if (span) {
      require(compare(span->from, span->to) < 0,
              "OriginalWorldBoundaryPositiveSpanRequired");
      // During an advance only its two real endpoints are observed. Calendar
      // readers may cover an older period prefix, so their query span is not
      // mistaken for the actually selected motion/work interval.
      require(compare(actual, span->from) == 0 ||
                  compare(actual, span->to) == 0,
              "OriginalWorldBoundaryOutsideSelectedEndpoints");
      require(compare(committed, span->from) == 0 ||
                  compare(committed, span->to) == 0,
              "OriginalWorldBoundaryPublishedMomentMismatch");
    }
    if (compare(actual, committed) != 0) {
      require(span && phase, "OriginalWorldBoundaryAdvanceProofRequired");
      require(compare(committed, span->from) == 0 &&
                  compare(actual, span->to) == 0,
              "OriginalWorldBoundaryUncommittedMomentMismatch");
      require(world_.common.actor_condition_capture_identity() != nullptr,
              "OriginalWorldBoundaryActualCaptureRequired");
    }
    return actual;
  });
}
Outcome<WorldMoment> OriginalWorldReadBoundary::read_committed_moment() const {
  return guard([&] {
    auto actual = take(read_observed_moment());
    auto committed =
        take(read_world_moment(world_.common.tick(), *world_.world_timeline));
    require(compare(actual, committed) == 0,
            "OriginalWorldBoundaryCommittedMomentRequired");
    const auto *writes = world_.active_exact_write_context();
    require(!writes || !writes->actual_advance_span(),
            "OriginalWorldBoundaryAdvanceCommandRejected");
    if (world_.active_read_phase_)
      require(compare(world_.active_read_phase_->span.from,
                      world_.active_read_phase_->span.to) == 0,
              "OriginalWorldBoundaryIntervalCommandRejected");
    return actual;
  });
}
} // namespace hanmo::resident
