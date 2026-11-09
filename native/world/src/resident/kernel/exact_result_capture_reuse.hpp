// Ephemeral P10 read-capture scratch; never a World field or persistent cache.
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <memory>
#ifdef HANMO_TEST_EXACT_CAPTURE_REUSE_ASSERT
#include <cstdio>
#endif
namespace hanmo::resident {
struct ExactResultCaptureScratch : CheckpointAllocatedObject {
  virtual ~ExactResultCaptureScratch() = default;
};
// A distinct frame for every call, including recursive/nested captures. The
// client additionally proves a live real ReadStamp, same World/owner/core/M.
// This frame is only the lexical lifetime boundary, never a validity proof.
class ExactResultCaptureReuse {
  ExactResultCaptureReuse *previous_;
public:
  static inline thread_local ExactResultCaptureReuse *active = nullptr;
  const void *const common;
  const WorldMoment &at;
  const void *client = nullptr;
  std::unique_ptr<ExactResultCaptureScratch> scratch;
#ifdef HANMO_TEST_EXACT_CAPTURE_REUSE_ASSERT
  std::size_t checked_hits = 0;
#endif
  ExactResultCaptureReuse(const void *root, const WorldMoment &actual)
      : previous_(active), common(root), at(actual) { active = this; }
  ~ExactResultCaptureReuse() {
#ifdef HANMO_TEST_EXACT_CAPTURE_REUSE_ASSERT
    if (checked_hits) std::fprintf(stderr,"P10_scope_reuse_assert checks=%zu\n",checked_hits);
#endif
    scratch.reset();
    active = previous_;
  }
  ExactResultCaptureReuse(const ExactResultCaptureReuse&) = delete;
  ExactResultCaptureReuse& operator=(const ExactResultCaptureReuse&) = delete;
};
}
