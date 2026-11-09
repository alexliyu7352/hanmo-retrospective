#include "native_field_military_command_route.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_application_read.hpp"
#include "native_field_group_world.hpp"
#include "native_field_world_scope.hpp"
#include "native_military_training_period.hpp"
#include "resident/domains/military/military_group_movement.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <new>
#include <stdexcept>

namespace hanmo::resident {
namespace {
// 只搬运原业务/资源错误；调用方保持整个候选原子弃用。
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
void require(bool value, const char *code) {
  if (!value)
    throw Failure{common_failure(code).error};
}
template <class T, class F> Outcome<T> guarded(F &&function) {
  try {
    return {true, function(), {}};
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldDepartureResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("FieldDepartureResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
// 所有可借根必须属于当前世界与本次原 owner；不从独立临时军事状态造来源。
const NativeCompleteWorldCoreSources &
bound(const WorldBundle &w, const CompleteExactWorldReadBindings &read) {
  require(w.catalog && w.catalog->military && w.catalog->field_battle &&
              w.field_battle && !w.military_exact && w.movement_exact &&
              w.spatial_carriers && w.group_movement_executions &&
              read.original_core &&
              &read.original_core->characters.early.world == &w &&
              &read.movement == &*w.movement_exact,
          "FieldDepartureOriginalCompleteOwnerRequired");
  const auto &core = *read.original_core;
  require(core.characters.early.formations && core.characters.early.returns &&
              core.characters.early.periods &&
              core.characters.early.field_application_clock,
          "FieldDepartureOriginalMilitarySourcesRequired");
  return core;
}
// 同地出征沿原 MilitaryCoordination 的零旅程语义；只借真实全军 manifest、
// 原稳定 writer 与精确原点，不把 Common 今日投影或 AlreadyThere 异常当成功。
std::optional<MilitaryJourneyRequest> original_stationary_departure(
    const WorldBundle &w, NativeFieldGroupWorld &group,
    const NativeCompleteWorldCoreSources &core, const StableId &actor,
    const CampaignDepart &command) {
  const CampaignRecord *campaign = nullptr;
  for (const auto &row : w.military.campaigns)
    if (row.id == command.campaign_id)
      campaign = &row;
  if (!campaign || campaign->status != "mustered")
    return std::nullopt; // 原跨城/重放路径保留自己的首错与历史采用。
  const MilitaryOrderDefinition *order = nullptr;
  for (const auto &row : w.catalog->military->orders)
    if (row.id == campaign->template_id)
      order = &row;
  const StrongholdDefinition *target = nullptr;
  if (order)
    for (const auto &row : w.catalog->military->strongholds)
      if (row.id == order->target_stronghold_id)
        target = &row;
  if (!target || !army_at_settlement(w.common, w.military, *w.catalog->travel,
                                     campaign->army_id, target->settlement_id))
    return std::nullopt;
  // 权限、mustered 与统帅占用仍先走原规则，后面只补源证明，绝不放宽报价。
  take(preview_military(w.common, w.military, *w.catalog->military,
                       core.characters.early.military_access, actor,
                       MilitaryCommand{command}));
  take(group.validate_bindings());
  require(!take(w.services.field_battle_bound(actor)),
          "FieldBattleMovementBlocked");
  const SpatialCarrierRef carrier = ArmyCarrierRef{campaign->army_id};
  const auto key = spatial_carrier_key(carrier);
  for (const auto &[id, journey] : w.spatial_carriers->journeys.rows())
    require(spatial_carrier_key(journey.carrier) != key ||
                w.spatial_carriers->terminals.find(id),
            "ArmyAlreadyHasMovementExecution");
  if (w.movement_exact->spatial_bindings.find(key)) {
    const auto writer = take(read_resolved_spatial_writer(*w.movement_exact, carrier));
    require(std::holds_alternative<StableMovementWriter>(writer.writer),
            "FieldDepartureStationaryOriginalWriterRequired");
  }
  const auto manifest = take(read_transport_manifest(
      w.common, *w.spatial_carriers, *w.movement_exact,
      group.group().carriers, carrier));
  const auto *settlement = w.catalog->travel->settlements.find(target->settlement_id);
  require(settlement, "UnknownSettlement");
  bool same = false;
  if (const auto *original = std::get_if<OriginalMovementAnchor>(&manifest.actual_anchor)) {
    if (const auto *city = std::get_if<SettlementLocation>(&original->location))
      same = city->settlement_id == target->settlement_id;
    else if (const auto *land = std::get_if<WildernessLocation>(&original->location))
      same = !w.catalog->travel->world_map_id.empty() &&
             land->map_id == w.catalog->travel->world_map_id &&
             land->point.x == settlement->map_xy.x && land->point.y == settlement->map_xy.y;
  } else if (const auto *land = std::get_if<TerrainContinuousAnchor>(&manifest.actual_anchor)) {
    same = !w.catalog->travel->world_map_id.empty() &&
           land->map_id == w.catalog->travel->world_map_id &&
           take(decode_movement_binary64(land->x_bits)) == settlement->map_xy.x &&
           take(decode_movement_binary64(land->y_bits)) == settlement->map_xy.y;
  }
  require(same, "FieldDepartureStationaryActualAnchorMismatch");
  return MilitaryJourneyRequest{campaign->army_id, target->settlement_id};
}
} // namespace

bool native_field_campaign_departure_selected(
    const WorldBundle &w, const MilitaryCommand &command) noexcept {
  return w.catalog && w.catalog->field_battle && w.catalog->exact_time &&
         exact_complete_selected(*w.catalog->exact_time) &&
         std::holds_alternative<CampaignDepart>(command);
}
Outcome<MilitaryQuote> preview_native_field_campaign_departure(
    const WorldBundle &w, const CompleteExactWorldReadBindings &read,
    const StableId &actor, const CampaignDepart &command) {
  return guarded<MilitaryQuote>([&] {
    const auto &core = bound(w, read);
    // 原权限/首错先走；之后构造真正 Army Group 借图，不预写状态或分配事实。
    auto quote =
        take(preview_military(w.common, w.military, *w.catalog->military,
                              core.characters.early.military_access, actor,
                              MilitaryCommand{command}));
    auto group = take(NativeFieldGroupWorld::create(w, core));
    take(group->validate_bindings());
    (void)original_stationary_departure(w, *group, core, actor, command);
    return quote;
  });
}
Outcome<MilitaryEffect> apply_native_field_campaign_departure_in_candidate(
    WorldBundle &w, const CompleteExactWorldReadBindings &read,
    const StableId &actor, const CampaignDepart &command) {
  return guarded<MilitaryEffect>([&] {
    const auto &core = bound(w, read);
    auto *writes = w.active_exact_write_context();
    require(writes && w.read_stamps &&
                w.common.actor_location_capture_identity() &&
                w.common.actor_condition_capture_identity(),
            "FieldDepartureActualCandidateCaptureRequired");
    const auto &early = core.characters.early;
    auto group = take(NativeFieldGroupWorld::create(w, core, &w));
    require(group->field().march_selection(),
            "FieldDepartureOriginalAuthorSelectionRequired");
    const MilitaryFieldHistoryRead history{*w.field_battle,
                                           *early.field_application_clock};
    const MilitarySelectedHistoryRead selected{w.catalog->identity, nullptr,
                                               &history};
    ReadStampWriteBoundary boundary(*w.read_stamps);
    if (const auto stationary = original_stationary_departure(w, *group, core, actor, command)) {
      // 原规则只置 marching；下一真实军务日历步写 arrival。这里零旅程、零费用。
      auto effect = take(apply_military_instant(
          w.common, w.military, *w.catalog->military, early.military_access,
          actor, MilitaryCommand{command}));
      require(effect.campaign_id == command.campaign_id && effect.journey_request &&
                  effect.journey_request->army_id == stationary->army_id &&
                  effect.journey_request->destination_settlement_id == stationary->destination_settlement_id,
              "FieldDepartureStationaryOriginalEffectMismatch");
      return effect;
    }
    const auto journey =
        take(start_selected_military_campaign_group_journey_in_candidate(
            w.common, w.military, *w.catalog->military, selected,
            early.military_access, group->stamps(),
            group->field().body_sources(), *group->field().march_selection(),
            *w.spatial_carriers, *w.movement_exact,
            *w.group_movement_executions, group->group(), command.campaign_id,
            actor, writes->spatial_location_writes,
            w.population_military_enabled() ? &w.population_military : nullptr,
            w.population_military_enabled()
                ? w.catalog->population_military.get()
                : nullptr,
            {}, early.formations, early.returns, early.periods));
    // 返回原接受行中的军团和目的地；不再调用 instant 或以结果 DTO
    // 再造第二行程。
    const GroupMovementOpeningSource *opening = nullptr;
    for (const auto &[id, row] : w.group_movement_executions->openings.rows()) {
      if (row.journey_id != journey)
        continue;
      require(!opening && row.military_order &&
                  row.military_order->kind ==
                      MilitaryMarchOrderKind::campaign_depart &&
                  row.military_order->campaign_id == command.campaign_id &&
                  row.military_order->issuing_actor_id == actor,
              "FieldDepartureActualOpeningMismatch");
      opening = &row;
    }
    require(opening, "FieldDepartureActualOpeningRequired");
    MilitaryEffect effect;
    effect.campaign_id = opening->military_order->campaign_id;
    effect.journey_request = MilitaryJourneyRequest{
        opening->military_order->army_id,
        opening->military_order->destination_settlement_id};
    return effect;
  });
}
} // namespace hanmo::resident
