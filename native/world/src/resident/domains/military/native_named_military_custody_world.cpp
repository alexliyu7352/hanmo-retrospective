#include "native_named_military_custody_world.hpp"
#include "military_formation_source.hpp"
#include "field_battle/field_segment_settlement.hpp"
#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/native_complete_world_core.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
namespace hanmo::resident {
namespace {
using namespace movement_detail;
// 内部借口只换真实完整释放拥有者；原 Common/Military/clock/Field 身份全部保留。
NamedMilitaryCustodyContext with_releases(NamedMilitaryCustodyContext x,
                                        const NamedMilitaryCustodyReleaseRead *releases) {
  x.releases = releases;
  return x;
}
void require_roots(const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
                   const NativeNamedMilitaryCustodyDependencies &d) {
  const auto &x = d.custody;
  if (!w.catalog || !w.catalog->common || !w.catalog->military ||
      !w.catalog->travel || !w.catalog->actor_foundation || !w.catalog->politics ||
      !w.actor_foundation || !w.movement_exact || !w.spatial_carriers ||
      !w.transport_registrations || &x.common != &w.common || &x.military != &w.military ||
      &x.catalog != w.catalog->military.get() || &x.identity != &w.catalog->identity ||
      !x.clock || !x.access || x.naval != (w.catalog->naval ? &w.naval : nullptr) ||
      x.field != (w.field_battle ? &*w.field_battle : nullptr))
    fail("NativeNamedMilitaryCustodyOriginalRootsRequired");
  const auto &early = core.characters.early;
  const auto &spatial = core.characters.spatial;
  if (&early.world != &w || x.clock != &early.clock || x.access != &early.military_access ||
      x.formations != early.formations || x.field_applications != early.field_application_clock ||
      &spatial.movement != &*w.movement_exact || &spatial.carriers != &*w.spatial_carriers ||
      &spatial.registrations != &*w.transport_registrations || &d.spatial != &spatial.original ||
      d.battle.legacy != spatial.battle.legacy || d.battle.field != spatial.battle.field ||
      d.military_handoffs != spatial.military_handoffs || d.births != spatial.births ||
      d.military_actions != spatial.military_actions)
    fail("CrossCandidateNamedMilitaryCustodyOriginalCore");
  if (x.formations && !x.formations->bound_to(w.common.state(), w.military))
    fail("CrossCandidateNamedMilitaryCustodyFormationRead");
  if (d.battle.legacy && !d.battle.legacy->bound_to(w.common, *w.movement_exact))
    fail("CrossCandidateNamedMilitaryCustodyBattleRead");
  if (d.battle.field && !d.battle.field->bound_to(w.common, *w.movement_exact))
    fail("CrossCandidateNamedMilitaryCustodyFieldRead");
  if (x.field && (!x.field_applications ||
      !x.field_applications->bound_to(w.common, w.military, *x.field) ||
      !d.battle.field || !d.field_exits))
    fail("NativeNamedMilitaryCustodyFieldSourcesRequired");
  if (d.field_exits && !d.field_exits->bound_to(w.common, *w.movement_exact))
    fail("CrossCandidateNamedMilitaryCustodyFieldExitRead");
  const NamedMilitaryCustodyTerritorySources territory(w, *x.clock, d.takeover);
  if (!territory.bound_to(w)) fail("CrossCandidateNamedMilitaryCustodyTerritory");
}
} // namespace
NativeNamedMilitaryCustodyWorldRead::NativeNamedMilitaryCustodyWorldRead(
    const WorldBundle &w, NativeNamedMilitaryCustodyDependencies d)
    : world_(w), territory_(w, *d.custody.clock, d.takeover),
      placements_(with_releases(d.custody, nullptr), *w.movement_exact),
      sources_{with_releases(d.custody, this), *w.catalog->common, *w.catalog->travel,
          territory_, *w.movement_exact, *w.spatial_carriers, *w.transport_registrations,
          *w.actor_foundation, *w.catalog->actor_foundation, d.spatial, d.battle, placements_,
          d.military_handoffs, w.catalog->family.get(), d.births, d.military_actions,
          d.budget, d.field_exits},
      releases_(sources_), availability_(sources_) {}
Outcome<std::unique_ptr<NativeNamedMilitaryCustodyWorldRead>>
make_native_named_military_custody_world_read(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    NativeNamedMilitaryCustodyDependencies d) {
  return movement_detail::guard<std::unique_ptr<NativeNamedMilitaryCustodyWorldRead>>([&] {
    if (&core.characters.early.world != &w || !w.catalog || !w.catalog->military ||
        &d.custody.common != &w.common ||
        &d.custody.military != &w.military || &d.custody.catalog != w.catalog->military.get() ||
        &d.custody.identity != &w.catalog->identity)
      movement_detail::fail("NativeNamedMilitaryCustodyOriginalRootsRequired");
    movement_detail::take(validate_named_military_custody_policy(d.custody));
    if (!movement_detail::take(selected_named_military_custody(d.custody.identity))) {
      // 未选也核真实原 Common 头反覆盖；null 不得藏住残留 term/release。
      movement_detail::take(validate_named_military_custody_terms(d.custody));
      for (const auto &[id, head] : w.common.state().facts.rows())
        if (head.kind == "military_named_custody_released")
          movement_detail::fail("NamedMilitaryCustodyReleasePayloadRequired", id);
      return std::unique_ptr<NativeNamedMilitaryCustodyWorldRead>{};
    }
    require_roots(w, core, d);
    return std::unique_ptr<NativeNamedMilitaryCustodyWorldRead>(
        new NativeNamedMilitaryCustodyWorldRead(w, d));
  });
}
bool NativeNamedMilitaryCustodyWorldRead::bound_to(
    const CommonStore &common, const MilitaryState &military) const noexcept {
  return &common == &world_.common && &military == &world_.military &&
      releases_.bound_to(common, military);
}
Outcome<const NamedMilitaryCustodyRelease *>
NativeNamedMilitaryCustodyWorldRead::read_current_named_custody_release(const StableId &actor) const {
  return releases_.read_current_named_custody_release(actor);
}
Status NativeNamedMilitaryCustodyWorldRead::validate_sources() const {
  auto valid = validate_named_military_custody_release_sources(sources_);
  return valid.ok ? validate_current_named_military_custody(sources_.custody) : valid;
}
Outcome<std::vector<const NamedMilitaryCustodyTerm *>>
NativeNamedMilitaryCustodyWorldRead::read_due_terms(const WorldMoment &from, const WorldMoment &to) const {
  return read_due_named_military_custody_terms(sources_, from, to);
}
const NamedMilitaryCustodyPlacementRead &NativeNamedMilitaryCustodyWorldRead::placements() const noexcept {
  return placements_;
}
const NativeNamedCustodyAvailabilityRead &NativeNamedMilitaryCustodyWorldRead::availability() const noexcept {
  return availability_;
}
const NamedMilitaryCustodyReleaseContext &NativeNamedMilitaryCustodyWorldRead::sources() const noexcept {
  return sources_;
}
} // namespace hanmo::resident
