#include "named_military_custody_territory.hpp"
#include "named_military_custody_history_model.hpp"
#include "resident/domains/politics/takeover/politics_takeover_absence.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
using namespace movement_detail;
NamedMilitaryCustodyTerritorySources::NamedMilitaryCustodyTerritorySources(
    const PoliticalTakeoverTerritoryAccess &legacy) : legacy_(&legacy) {}
NamedMilitaryCustodyTerritorySources::NamedMilitaryCustodyTerritorySources(
    const WorldBundle &world, const CurrentMomentRead &clock,
    const PoliticalTakeoverContext *takeover)
    : world_(&world), clock_(&clock), takeover_(takeover) {}
bool NamedMilitaryCustodyTerritorySources::bound_to(const WorldBundle &w) const noexcept {
  if (world_ != &w || !clock_ || !w.catalog || !w.catalog->common ||
      !w.catalog->politics || !w.catalog->military) return false;
  const bool selected = w.catalog->political_takeover && w.catalog->political_takeover->enabled;
  if (selected != bool(takeover_)) return false;
  if (!takeover_) return true;
  const auto &t = *takeover_;
  return w.catalog->organization && w.organization_runtime &&
      &t.common == &w.common && &t.common_catalog == w.catalog->common.get() &&
      &t.organizations == &w.organization && &t.organization_catalog == w.catalog->organization.get() &&
      &t.politics_catalog == w.catalog->politics.get() &&
      &t.catalog == w.catalog->political_takeover.get() && &t.state == &w.political_takeover &&
      &t.military == &w.military && &t.military_catalog == w.catalog->military.get() &&
      t.politics_state == &w.politics && t.organization_runtime == &*w.organization_runtime &&
      t.actual_clock;
}
Status NamedMilitaryCustodyTerritorySources::validate_native_roots() const {
  if (!world_ || !bound_to(*world_))
    return common_failure("CrossCandidateNamedMilitaryCustodyTerritory");
  // 全部原选择、表、Common 头、命令与后继反残留沿既有入口，不以空表推未选。
  return takeover_ ? common_success() : validate_unselected_political_takeover_sources(*world_);
}
Outcome<const StableId *> NamedMilitaryCustodyTerritorySources::military_settlement_jurisdiction(
    const StableId &id) const {
  if (legacy_) return legacy_->military_settlement_jurisdiction(id);
  return guard<const StableId *>([&] {
    take(validate_native_roots());
    if (takeover_) return take(PoliticalTakeoverTerritoryAccess(*takeover_).military_settlement_jurisdiction(id));
    const auto &catalog = *world_->catalog->politics;
    if (!catalog.enabled || id.empty())
      throw Failure{common_failure("MilitarySettlementJurisdictionOriginalRequired", id,
                                   ErrorKind::unsupported_capability).error};
    const StableId *selected = nullptr;
    for (const auto &[key, row] : catalog.jurisdictions.rows()) {
      const auto *seat = world_->catalog->common->places.find(row.seat_place_id);
      if (key != row.id || row.id.empty() || row.revision != 0 || !seat ||
          seat->id != row.seat_place_id || seat->settlement_id.empty())
        fail("MilitarySettlementJurisdictionSourceMismatch", key);
      if (seat->settlement_id != id) continue;
      if (selected) fail("MilitarySettlementJurisdictionAmbiguous", id);
      selected = &row.id;
    }
    if (!selected)
      throw Failure{common_failure("MilitarySettlementJurisdictionOriginalRequired", id,
                                   ErrorKind::unsupported_capability).error};
    return selected;
  });
}
Outcome<PoliticalTerritoryControl> NamedMilitaryCustodyTerritorySources::
    governance_jurisdiction_control_current(const StableId &id) const {
  if (legacy_) return legacy_->governance_jurisdiction_control_current(id);
  return guard<PoliticalTerritoryControl>([&] {
    take(validate_native_roots());
    if (takeover_) return take(PoliticalTakeoverTerritoryAccess(*takeover_).governance_jurisdiction_control_current(id));
    const auto *seed = world_->catalog->politics->jurisdictions.find(id);
    const auto *actual = world_->politics.jurisdictions.find(id);
    if (!seed || seed->id != id || seed->revision != 0 || seed->controlling_organization_id.empty())
      fail("GovernanceJurisdictionOriginalMissing", id);
    if (!actual || actual->id != id || actual->revision != 0 ||
        actual->controlling_organization_id != seed->controlling_organization_id ||
        actual->political_organization_id != seed->political_organization_id ||
        actual->office_id != seed->office_id || actual->seat_place_id != seed->seat_place_id ||
        actual->public_wallet_id != seed->public_wallet_id ||
        actual->public_inventory_ids != seed->public_inventory_ids)
      fail("GovernanceJurisdictionCurrentMismatch", id);
    return PoliticalTerritoryControl{world_->common.state().identity, id,
                                    seed->controlling_organization_id, {}, 0};
  });
}
Outcome<PoliticalTerritoryControl> NamedMilitaryCustodyTerritorySources::
    military_jurisdiction_control_before_custody_release(
        const NamedMilitaryCustodyRelease &release, const StableId &id) const {
  if (legacy_) return legacy_->military_jurisdiction_control_before_custody_release(release, id);
  return guard<PoliticalTerritoryControl>([&] {
    take(validate_native_roots());
    if (takeover_) return take(PoliticalTakeoverTerritoryAccess(*takeover_)
        .military_jurisdiction_control_before_custody_release(release, id));
    const auto &w = *world_;
    const NamedMilitaryCustodyRelease *actual = nullptr;
    const NamedMilitaryCustodyTerm *term = nullptr;
    for (const auto &row : w.military.named_custody.closures) if (row.id == release.id) {
      if (actual) fail("MilitaryCustodyReleaseSourceAmbiguous", release.id);
      actual = &row;
    }
    for (const auto &row : w.military.named_custody.terms) if (row.id == release.term_id) {
      if (term) fail("MilitaryCustodyTermSourceAmbiguous", release.term_id);
      term = &row;
    }
    if (actual != &release || !term || term->actor_id != release.actor_id ||
        release.recorded_tick != release.released_at.half_day_tick)
      fail("MilitaryCustodyReleaseActualSourceRequired", release.id);
    const auto cut = take(read_actual_common_fact_position(w.common,
        SourceRef{"military", "military_named_custody_released", release.id}));
    if (take(read_actual_common_fact_position(w.common,
            SourceRef{"military", "military_named_custody_term", term->id})) >= cut ||
        take(read_actual_common_fact_position(w.common, release.prior_custody_source)) >= cut)
      fail("MilitaryCustodyReleaseControlCutMismatch", release.id);
    if (take(compare_world_moments(release.released_at, take(clock_->read_current_moment()))) > 0)
      fail("MilitaryCustodyReleaseInFuture", release.id);
    // 原控制唯一 writer 已被完整未选证明排除，历史值为不可变作者 seed，非今日倒填。
    return take(governance_jurisdiction_control_current(id));
  });
}
} // namespace hanmo::resident
