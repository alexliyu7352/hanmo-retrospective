#include "native_field_military_schedule.hpp"
#include "common_command_sources.hpp"
#include "complete_task_command_binding.hpp"
#include "dispatcher_exact_bindings.hpp"
#include "dynamic_integration.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "resident/domains/military/military_schedule.hpp"
#include "world_bundle.hpp"
#include <limits>
#include <new>
#include <set>
#include <stdexcept>

namespace hanmo::resident {
namespace {
// 不转换业务首错，也不在缺少新 Group 根的 legacy savepoint 下保留半次命令。
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
    throw Failure{common_failure(code, {}, ErrorKind::internal).error};
}
// 本 fixture
// 没有选择返营/攻城/海军；真实提案若走到它们，明确拒绝而不是换成出征。
CommandPayload original_command(const ResidentMilitaryProposal &proposal) {
  return std::visit(
      [](const auto &payload) -> CommandPayload {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, MilitaryCommand>) {
          StableId kind;
          if (std::holds_alternative<OfficeAccept>(payload))
            kind = "office_accept";
          else if (std::holds_alternative<CampaignAccept>(payload))
            kind = "campaign_accept";
          else if (std::holds_alternative<ArmyForm>(payload))
            kind = "army_form";
          else if (std::holds_alternative<CampaignDepart>(payload))
            kind = "campaign_depart";
          else
            throw Failure{
                common_failure("FieldMilitaryPlanSelectedCommandRequired", {},
                               ErrorKind::unsupported_capability)
                    .error};
          return {std::move(kind), payload};
        } else if constexpr (std::is_same_v<T, TravelCommandPayload>) {
          if (std::holds_alternative<MoveLocalCommand>(payload))
            return {"move_local", payload};
          if (std::holds_alternative<TravelCommand>(payload))
            return {"travel", payload};
          throw Failure{
              common_failure("FieldMilitaryPlanOriginalTravelRequired", {},
                             ErrorKind::unsupported_capability)
                  .error};
        } else {
          throw Failure{common_failure("FieldMilitaryPlanNavalOwnerRequired",
                                       {}, ErrorKind::unsupported_capability)
                            .error};
        }
      },
      proposal.command);
}
} // namespace

Status
run_native_field_military_plans_in_candidate(WorldBundle &w,
                                             ExactPhaseWriteContext &writes) {
  try {
    require(w.catalog && w.catalog->military && w.catalog->field_battle &&
                w.field_battle && w.world_timeline &&
                w.world_timeline->boundary_state ==
                    PeriodBoundaryState::ready &&
                w.active_exact_write_context() == &writes &&
                !writes.actual_advance_span() &&
                w.common.actor_condition_capture_identity() &&
                w.common.actor_location_capture_identity(),
            "FieldMilitaryPlanActualPostMacroRequired");
    const auto at = take(w.actual_phase_moment());
    require(at.half_day_tick == w.common.tick() &&
                take(compare_fractions(at.sub_half_day, CanonicalFraction{})) ==
                    0,
            "FieldMilitaryPlanActualIntegerMomentRequired");
    auto phase = take(w.enter_phase_read_context("field_military_plan",
                                                 "military", {at, at}, at));
    std::vector<ResidentMilitaryProposal> proposals;
    {
      auto owner = take(w.complete_exact_time_bindings());
      require(owner && owner->reads().original_core &&
                  &owner->reads().original_core->characters.early.world == &w,
              "FieldMilitaryPlanOriginalOwnerRequired");
      auto access = w.services.interaction_access(&owner->reads());
      auto military =
          owner->reads().original_core->characters.early.military_access;
      military.interaction_access = &access;
      proposals = take(military_schedule_proposals(
          w.common, w.military, *w.catalog->military, *w.catalog->common,
          *w.catalog->travel, access, w.common.state().metadata.player_actor_id,
          military, w.naval_enabled() ? &w.naval : nullptr,
          w.naval_enabled() ? w.catalog->naval.get() : nullptr,
          w.naval_combat_enabled(),
          w.naval_combat_enabled() ? w.catalog->naval.get() : nullptr));
    }
    std::set<StableId> actors;
    for (const auto &proposal : proposals) {
      require(actors.insert(proposal.actor_id).second &&
                  proposal.actor_id !=
                      w.common.state().metadata.player_actor_id,
              "FieldMilitaryPlanDuplicateOrPlayerProposal");
      if (director_routes_actor(w, proposal.actor_id))
        continue;
      const auto command = original_command(proposal);
      // 每一提案重借当前真源；前一命令完成后不使用过期的 read-stamp 快照。
      auto owner = take(w.complete_exact_time_bindings());
      auto prepared = prepare_command_with_exact_bindings(
          w, proposal.actor_id, command, owner->reads());
      if (!prepared.ok) {
        if (prepared.error.kind == ErrorKind::rule)
          continue;
        throw Failure{std::move(prepared.error)};
      }
      require(prepared.value && prepared.value->operation &&
                  prepared.value->actor_id == proposal.actor_id &&
                  prepared.value->kind == command.kind,
              "FieldMilitaryPlanActualPreparedCommandRequired");
      const auto *old =
          w.common.state().request_sequences.find(proposal.actor_id);
      require(old && *old >= 0 && *old < 2147483647,
              "FieldMilitaryPlanSequenceExhausted");
      const Integer sequence = *old + 1;
      auto applied = take(apply_command_with_exact_bindings(
          w, proposal.actor_id, *prepared.value->operation, owner->reads()));
      require(w.growth_market_capture.open_scopes == 0 &&
                  !w.growth_market_capture.unbound_business,
              "GrowthMarketUnboundBusiness");
      auto receipt = take(seal_complete_command_result_in_candidate(
          w, proposal.actor_id, sequence, command.kind, applied));
      take(bind_complete_task_command_result(w, *receipt));
      require(take(compare_world_moments(take(w.actual_phase_moment()), at)) ==
                  0,
              "FieldMilitaryPlanChangedWorldTime");
    }
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldMilitaryPlanResourceExhausted", {},
                          ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("FieldMilitaryPlanResourceExhausted", {},
                          ErrorKind::internal);
  }
}
} // namespace hanmo::resident
