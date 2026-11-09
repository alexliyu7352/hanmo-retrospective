// Pure early same-World Field application clock. It borrows the original S0
// proof, not Body, complete getters, a copied timestamp, or another World.
#pragma once
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
namespace hanmo::resident {
struct WorldBundle;
class OriginalWorldReadBoundary;
class NativeFieldApplicationRead final : public FieldSegmentApplicationRead {
  const WorldBundle &world_;
  const CurrentMomentRead &clock_;
  const OriginalWorldReadBoundary &boundary_;

public:
  NativeFieldApplicationRead(const WorldBundle &world,
                             const CurrentMomentRead &clock,
                             const OriginalWorldReadBoundary &boundary)
      : world_(world), clock_(clock), boundary_(boundary) {}
  NativeFieldApplicationRead(const NativeFieldApplicationRead &) = delete;
  NativeFieldApplicationRead &
  operator=(const NativeFieldApplicationRead &) = delete;
  bool bound_to(const CommonStore &, const MilitaryState &,
                const FieldBattleWorldState &) const noexcept override;
  Outcome<WorldMoment> read_current_moment() const override;
  Outcome<WorldMoment> read_field_window_closing_moment() const override;
};
} // namespace hanmo::resident
