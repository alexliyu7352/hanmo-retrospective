#include "resident/domains/military/military_model.hpp"
// 真关闭头先连接实际 S1 位置与 S4 因果链；只移除当前占用，保原俘获历史。
#include "named_military_custody_expiry.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
MovementAnchor released_anchor(const MovementAnchor &origin,
    const NamedMilitaryCustodyDestination &destination, const SourceRef &source) {
  if (destination.destination)
    return OriginalMovementAnchor{*destination.destination, source};
  // 原地回退也保原有理道路进度；不能先投影整数再伪称同一位置。
  if (const auto *ordinary = std::get_if<OriginalMovementAnchor>(&origin))
    return OriginalMovementAnchor{ordinary->location, source};
  if (const auto *road = std::get_if<RoadFractionalAnchor>(&origin)) {
    auto result = *road;
    result.actual_source = source;
    return result;
  }
  fail("NamedMilitaryCustodyOriginalWorldAnchorRequired");
}
void remove_original_occupancy(CommonStore &common, MilitaryState &military,
    const NamedMilitaryCustodyView &custody) {
  Integer found = 0;
  for (auto &army : military.armies) {
    const auto count = std::count(army.prisoner_ids.begin(), army.prisoner_ids.end(), custody.actor_id);
    if (!count) continue;
    if (custody.garrison_id || army.id != custody.captor_army_id || count != 1)
      fail("NamedMilitaryCustodyReleaseOccupancyMismatch", custody.actor_id);
    army.prisoner_ids.erase(std::find(army.prisoner_ids.begin(), army.prisoner_ids.end(), custody.actor_id));
    common.invalidate({ResourceKey::Kind::army, army.id});
    ++found;
  }
  for (auto &garrison : military.garrisons) {
    const auto count = std::count(garrison.prisoner_ids.begin(), garrison.prisoner_ids.end(), custody.actor_id);
    if (!count) continue;
    if (!custody.garrison_id || garrison.id != *custody.garrison_id || count != 1)
      fail("NamedMilitaryCustodyReleaseOccupancyMismatch", custody.actor_id);
    garrison.prisoner_ids.erase(std::find(garrison.prisoner_ids.begin(), garrison.prisoner_ids.end(), custody.actor_id));
    common.invalidate({ResourceKey::Kind::army, custody.captor_army_id});
    ++found;
  }
  if (found != 1) fail("NamedMilitaryCustodyReleaseOccupancyMismatch", custody.actor_id);
}
} // namespace
Outcome<NamedMilitaryCustodyExpiryApplication> expire_named_military_custody_in_candidate(
    CommonStore &common, MilitaryState &military, AuthoredActorFoundationState &foundation,
    MovementExactState &movement, const NamedMilitaryCustodyExpiryContext &write,
    const StableId &term_id) {
  return guard<NamedMilitaryCustodyExpiryApplication>([&] {
    const auto &x = write.sources;
    if (&common != &x.custody.common || &military != &x.custody.military ||
        &foundation != &x.foundation || &movement != &x.movement ||
        &write.conditions.common != &common || &write.conditions.state != &foundation ||
        !x.custody.clock || !x.custody.releases ||
        !x.custody.releases->bound_to(common, military) || !x.placements.bound_to(common, movement))
      fail("CrossCandidateNamedMilitaryCustodyExpiry", term_id);
    take(validate_named_military_custody_release_sources(x));
    take(validate_military_availability_coverage(write.conditions));
    const auto *term = take(read_original_named_military_custody_term(
        x.custody, {"military", "military_named_custody_term", term_id}));
    for (const auto &release : military.named_custody.closures) if (release.term_id == term_id) {
      take(read_original_named_military_custody_release(x,
          {"military", "military_named_custody_released", release.id}));
      NamedMilitaryCustodyExpiryApplication result;
      result.closure_id = release.id;
      result.already_applied = true;
      result.availability.already_applied = true;
      return result;
    }
    const auto now = take(x.custody.clock->read_current_moment());
    if (now.half_day_tick != common.tick() ||
        take(compare_world_moments(now, write.conditions.actual_now, x.budget)) != 0 ||
        take(compare_world_moments(now, term->due_at, x.budget)) != 0)
      fail("NamedMilitaryCustodyActualDeadlineRequired", term_id);
    const auto current = take(read_current_named_military_custody(x.custody, term->actor_id));
    if (!current) fail("NamedMilitaryCustodyCurrentSourceRequired", term->actor_id);
    // 原运输登记若仍占这个人，必须先有真实登记解除来源；不只删本人的 writer。
    if (take(read_actual_transport_participation_presence(x.carriers, x.spatial, term->actor_id)))
      fail("NamedMilitaryCustodyOriginalTransportDetachRequired", term->actor_id);
    const auto *person = common.actor(term->actor_id);
    if (!person || !person->alive || !person->action_id.empty())
      fail("NamedMilitaryCustodyReleaseActorSourceRequired", term->actor_id);
    const auto prefix = take(read_current_common_fact_prefix(common));
    CommonActualFactPrefix actual_prefix;
    actual_prefix.common_fact_count = prefix.common_fact_count;
    if (prefix.last_common_fact)
      actual_prefix.last_common_fact = SourceRef{prefix.last_common_fact->domain,
          prefix.last_common_fact->kind, prefix.last_common_fact->id};
    const auto origin = take(read_actor_spatial_location_at_actual_fact_prefix(
        common, x.common_catalog, movement, x.carriers, x.registrations, foundation,
        x.actors, x.spatial, term->actor_id, now, actual_prefix, x.military_handoffs,
        x.budget, x.family_catalog, x.births, x.military_actions, &x.placements, x.field_exits));
    const auto *ordinary = take(require_actor_ordinary_movement_anchor(origin));
    if (origin.actual_location_sources.empty())
      fail("NamedMilitaryCustodyReleaseOriginRequired", term->actor_id);
    const auto destination = take(quote_named_military_custody_destination(
        x.custody, x.travel, x.territory, *term, *ordinary, x.budget));
    if (!common.actor_location_capture_identity() || !common.actor_condition_capture_identity())
      fail("NamedMilitaryCustodyActualCaptureRequired", term->actor_id);
    NamedMilitaryCustodyRelease release;
    release.id = take(common.next_id("named_military_release"));
    const SourceRef source{"military", "military_named_custody_released", release.id};
    release.term_id = term_id;
    release.actor_id = term->actor_id;
    release.keeper_army_id = current->captor_army_id;
    for (const auto &army : military.armies) if (army.id == release.keeper_army_id)
      release.keeper_organization_id = army.organization_id;
    release.prior_custody_source = current->current_custody_source;
    release.released_at = now;
    release.recorded_tick = common.tick();
    release.origin_anchor = *ordinary;
    for (const auto &original : origin.actual_location_sources)
      release.origin_sources.push_back({original.source, original.schema});
    release.destination_priority = destination.priority;
    release.destination_settlement_id = destination.settlement_id;
    release.jurisdiction_id = destination.jurisdiction_id;
    release.road_distance = destination.road_distance;
    release.road_ids = destination.road_ids;
    release.initial_returning = destination.initial_returning;
    release.released_anchor = released_anchor(*ordinary, destination, source);
    take(common.append_fact({release.id, source.kind, source.domain}));
    military.named_custody.closures.push_back(std::move(release));
    const auto &stored = military.named_custody.closures.back();
    remove_original_occupancy(common, military, *current);
    movement.spatial_bindings.set("actor:" + stored.actor_id,
        {"actor", stored.actor_id, StableMovementWriter{stored.released_anchor}, source});
    const auto offset = common.actor_location_execution_drafts().size();
    if (offset >= static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
      fail("NamedMilitaryCustodyLocationCaptureOverflow", stored.actor_id);
    ActorMutation mutation;
    mutation.actor_id = stored.actor_id;
    mutation.location = take(project_movement_location(stored.released_anchor, x.budget));
    take(common.apply_actor(mutation));
    NamedMilitaryCustodyExpiryApplication result;
    result.closure_id = stored.id;
    result.location_writes.push_back(take(bind_actual_named_custody_location_write(
        common, movement, x.placements, stored.actor_id, source, stored.released_anchor,
        static_cast<Integer>(offset + 1), x.budget)));
    // S4 自己沿首次捕获的原前驱决定恢复资格；军务既不传 true，也不写第二次 available。
    const auto stamp = take(write.reads.capture({{ResourceKey::Kind::actor, stored.actor_id}}, {}, {stored.id}));
    if (!stamp) fail("NamedMilitaryCustodyActualReadStampRequired", stored.actor_id);
    result.availability = take(apply_verified_named_custody_release(common, foundation,
        write.conditions, source, *stamp, write.authority, write.versions));
    take(validate_named_military_custody_release_sources(x));
    take(validate_military_availability_coverage(write.conditions));
    take(validate_current_named_military_custody(x.custody));
    return result;
  });
}
} // namespace hanmo::resident
