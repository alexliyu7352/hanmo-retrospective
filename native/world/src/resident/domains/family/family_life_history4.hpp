// Borrow the actual complete Life4 owner for original Family obligations.
// The original per-obligation fields are unchanged; no legacy military origin
// is manufactured from a natural-age or named sea death.
#pragma once
#include "family_life_exact.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
class FamilyLifeHistoryRead4 final {
  const CommonStore &common_;
  const FamilyDeathState &deaths_;
  const FamilyDeathPolicy &policy_;
  const CatalogIdentity &catalog_;
  const FamilyLifeCauseSources4 &sources_;
  const CurrentMomentRead &clock_;
  Status selected() const;
  Outcome<const FamilyDeathRecord *>
  read_actor_death_record(const StableId &, const StableId *&actual_key) const;
  Outcome<std::optional<VerifiedDeathView>>
  death_at_cut(const StableId &, Integer, std::size_t) const;

public:
  FamilyLifeHistoryRead4(const CommonStore &c, const FamilyDeathState &d,
                         const FamilyDeathPolicy &p, const CatalogIdentity &i,
                         const FamilyLifeCauseSources4 &s,
                         const CurrentMomentRead &t)
      : common_(c), deaths_(d), policy_(p), catalog_(i), sources_(s),
        clock_(t) {}
  bool bound_to(const CommonStore &c) const noexcept { return &common_ == &c; }
  bool bound_to(const CommonStore &c,
                const FamilyDeathState &d) const noexcept {
    return &common_ == &c && &deaths_ == &d;
  }
  Status validate() const;
  Outcome<WorldMoment> read_current_moment() const;
  Outcome<std::optional<VerifiedDeathView>>
  read_actor_death_before_original_fact(const StableId &,
                                        Integer projected_tick,
                                        const SourceRef &original_fact) const;
  Outcome<std::optional<VerifiedDeathView>>
  read_actor_death(const StableId &) const;
  Outcome<std::optional<VerifiedDeathView>>
  read_actor_death_at_original_prefix(const StableId &, Integer,
                                      const CommonActualFactPrefix &) const;
  // 原采用 owner 必须证明该 prefix 在 actual M 被真实捕获；本机械口不授予
  // 调用者自选历史时刻的权力。返回仅借本 Family 根，null 表示切点尚无死。
  Outcome<const FamilyDeathRecord *> read_actor_death_record_at_original_prefix(
      const StableId &, const WorldMoment &actual_at,
      const CommonActualFactPrefix &) const;
  // projected_tick remains the original obligation's integer projection. Same
  // half-day causality is the actual retained Common append cut, never ID order
  // or an invented exact moment for the obligation.
  Status
  require_no_death_before_original_fact(const StableId &actor,
                                        Integer projected_tick,
                                        const SourceRef &original_fact) const;
  Status
  require_death_before_original_fact(const DeathRef &, const StableId &actor,
                                     Integer projected_tick,
                                     const SourceRef &original_fact) const;
};
} // namespace hanmo::resident
