// S4 只借原军务 term/closure 完整正文；不建立另一份在押、释放或可用性账。
#pragma once
#include "resident/domains/military/named_military_custody_release_read.hpp"
#include "resident/domains/military/named_military_custody_terms.hpp"
namespace hanmo::resident {
class NamedCustodyAvailabilitySourceSink {
public:
  virtual ~NamedCustodyAvailabilitySourceSink() = default;
  // 同步只读借用原拥有行，回调不得修改任何候选根。
  virtual Status capture(const NamedMilitaryCustodyTerm &) = 0;
  virtual Status release(const NamedMilitaryCustodyRelease &) = 0;
};
class NativeNamedCustodyAvailabilityRead final {
  NamedMilitaryCustodyReleaseContext x_;

public:
  explicit NativeNamedCustodyAvailabilityRead(
      NamedMilitaryCustodyReleaseContext x)
      : x_(x) {}
  bool bound_to(const CommonStore &, const MilitaryState &,
                const AuthoredActorFoundationState &,
                const ActorFoundationCatalog &) const noexcept;
  Outcome<const NamedMilitaryCustodyTerm *>
  read_capture(const SourceRef &) const;
  Outcome<const NamedMilitaryCustodyRelease *>
  read_release(const SourceRef &) const;
  Status visit_original_sources(NamedCustodyAvailabilitySourceSink &) const;
};
} // namespace hanmo::resident
