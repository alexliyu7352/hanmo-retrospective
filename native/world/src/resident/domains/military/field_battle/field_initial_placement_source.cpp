#include "field_validation.hpp"
#include "field_segment_settlement.hpp"
#include "field_initial_placement_source.hpp"
#include "field_ability_sources.hpp"
#include "field_command_source.hpp"
#include "field_council_validation.hpp"
#include "field_deployment.hpp"
#include "field_exact_contact_source.hpp"
#include "field_flank.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_roster_origin.hpp"
#include "field_started_source.hpp"
#include "field_view_model.hpp"
#include "resident/domains/military/military_model.hpp"
namespace hanmo::resident {
Outcome<FieldInitialPlacementRead> read_field_original_initial_placement(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const FieldBattleSession &session,
    const FieldFormation &formation, const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &sources) {
  auto fail = [&](const char *code) -> Outcome<FieldInitialPlacementRead> {
    return {false, {}, common_failure(code, formation.formation_id).error};
  };
  if (!sources.original_rosters || !sources.exact_contacts ||
      !sources.original_formations || !session.started_source ||
      session.field_hash != field.field_hash ||
      !sources.exact_contacts->clock.bound_to(common, military, world))
    return fail("FieldInitialPlacementOriginalOwnersRequired");
  auto start = read_field_battle_started_source(
      common, military, world, *session.started_source,
      sources.exact_contacts->clock, *sources.original_formations);
  if (!start.ok)
    return {false, {}, start.error};
  if (start.value.session != &session ||
      !session.formations.contains(formation.formation_id) ||
      &session.formations.at(formation.formation_id) != &formation)
    return fail("FieldInitialPlacementOriginalSessionMismatch");
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : world.contacts)
    if (row.battle_id == session.battle_id) {
      if (contact)
        return fail("FieldInitialPlacementOriginalContactAmbiguous");
      contact = &row;
    }
  if (!contact)
    return fail("FieldInitialPlacementOriginalContactRequired");
  std::set<StableId> ids;
  for (const auto &binding : contact->original_bindings)
    for (const auto &member : binding.members)
      ids.insert(member.formation_id);
  if (!ids.contains(formation.formation_id)) {
    auto entry = read_field_reinforcement_admission_identity(
        common, world, session, formation);
    if (!entry.ok)
      return {false, {}, entry.error};
    auto roster = read_field_original_participation_roster(
        common, military, sources.military, world, session, formation,
        *sources.original_rosters);
    if (!roster.ok)
      return {false, {}, roster.error};
    if (formation.fit_start != Integer(roster.value.fit.size()))
      return fail("FieldInitialPlacementOriginalFitMismatch");
    return {true,
            {entry.value->position_dm,
             entry.value->facing_mdeg,
             nullptr,
             nullptr,
             &entry.value->application_source,
             &entry.value->initial_mission,
             nullptr,
             {},
             entry.value},
            {}};
  }
  for (const auto &id : ids) {
    auto found = session.formations.find(id);
    if (found == session.formations.end())
      return fail("FieldInitialPlacementOriginalRosterMissing");
    auto row = read_field_original_participation_roster(
        common, military, sources.military, world, session, found->second,
        *sources.original_rosters);
    if (!row.ok)
      return {false, {}, row.error};
    if (found->second.fit_start != Integer(row.value.fit.size()))
      return fail("FieldInitialPlacementOriginalFitMismatch");
  }
  if (formation.deployment_command_result) {
    auto source =
        read_field_original_deployment_application(common, session, formation);
    if (!source.ok)
      return {false, {}, source.error};
    return {
        true,
        {source.value->position_dm,
         source.value->facing_mdeg,
         nullptr,
         source.value,
         &*formation.deployment_command_result,
         &source.value->immediate_mission,
         source.value->wave_mission ? &*source.value->wave_mission : nullptr,
         {}},
        {}};
  }
  const FieldCouncilFact *adoption = nullptr;
  for (const auto &[id, row] : world.council_facts)
    if (row.battle_id == session.battle_id && row.side == formation.side &&
        row.kind == FieldCouncilFactKind::Adopted) {
      if (adoption)
        return fail("FieldInitialPlacementCouncilAmbiguous");
      adoption = &row;
    }
  if (!adoption)
    return fail("FieldInitialPlacementCouncilRequired");
  const FieldCouncilPlan *plan = nullptr;
  for (const auto &proposal : adoption->council.proposals)
    if (adoption->council.adopted_proposal_id == proposal.proposal_id) {
      if (plan)
        return fail("FieldInitialPlacementPlanAmbiguous");
      plan = &proposal.plan;
    }
  if (!plan)
    return fail("FieldInitialPlacementPlanRequired");
  const FieldCouncilAssignment *assignment = nullptr;
  for (const auto &a : plan->assignments)
    if (a.formation_id == formation.formation_id) {
      if (assignment)
        return fail("FieldInitialPlacementAssignmentAmbiguous");
      assignment = &a;
    }
  if (!assignment)
    return fail("FieldInitialPlacementAssignmentRequired");
  FieldAiStrategy strategy;
  switch (plan->layout) {
  case FieldCouncilLayout::Hold:
    strategy = FieldAiStrategy::HoldGround;
    break;
  case FieldCouncilLayout::Line:
  case FieldCouncilLayout::Ambush:
    strategy = FieldAiStrategy::LineReserve;
    break;
  case FieldCouncilLayout::Flank:
    strategy = FieldAiStrategy::Flanking;
    break;
  default:
    return fail("FieldInitialPlacementLayoutInvalid");
  }
  auto layout = prepare_field_original_deployment_layout(
      session, field, formation.side, strategy, ids);
  if (!layout.ok)
    return {false, {}, layout.error};
  for (const auto &placed : layout.value.placements)
    if (placed.formation_id == formation.formation_id) {
      if (assignment->flank_followup) {
        if (assignment->wave == FieldCouncilWave::Reserve)
          return fail("FieldInitialFlankWaveInvalid");
        auto flank =
            read_field_flank_target(session, field, formation.side, placed);
        if (!flank.ok)
          return {false, {}, flank.error};
        if (!same_field_battle_mission(assignment->mission,
                                       FieldMission{flank.value}))
          return fail("FieldInitialFlankGeometryMismatch");
      }
      PointI point = placed.point_dm;
      Integer facing = placed.facing_mdeg;
      for (const auto &item : plan->stratagems)
        if (!item.voided_at_adoption &&
            item.kind == FieldCouncilStratagemKind::Ambush &&
            item.formation_id == formation.formation_id) {
          const auto *hold = std::get_if<FieldHold>(&assignment->mission);
          if (!hold)
            return fail("FieldInitialPlacementAmbushMissionRequired");
          point = hold->point_dm;
          facing = hold->facing_mdeg;
        }
      FieldInitialPlacementRead result{point, facing, adoption, nullptr,
                                       nullptr};
      if (assignment->wave == FieldCouncilWave::Second) {
        result.wave_mission = &assignment->mission;
        result.original_second_wave_hold =
            FieldHold{placed.point_dm, placed.facing_mdeg};
      } else
        result.immediate_mission = &assignment->mission;
      return {true, result, {}};
    }
  return fail("FieldInitialPlacementFormationMissing");
}
} // namespace hanmo::resident
