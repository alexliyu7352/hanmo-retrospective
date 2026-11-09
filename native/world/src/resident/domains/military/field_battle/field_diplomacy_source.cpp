#include "resident/domains/military/military_terms.hpp"
#include "resident/domains/military/military_model.hpp"
#include "field_diplomacy_source.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
namespace {
Outcome<FieldReinforcementDiplomacyRead>
read_alignment(const PoliticalRelationsReadContext &x, const StableId &battle,
               const StableId &army_id, const SourceRef *before,
               ExactArithmeticBudget budget) {
  auto fail =
      [&](const char *code) -> Outcome<FieldReinforcementDiplomacyRead> {
    return {false, {}, common_failure(code, army_id).error};
  };
  if (!x.actual_clock.bound_to(x.common, x.military, x.field) ||
      !x.formations.bound_to(x.common.state(), x.military))
    return fail("FieldDiplomacyOriginalOwnerRequired");
  const auto session = x.field.sessions.find(battle);
  if (session == x.field.sessions.end() ||
      session->second.battle_id != battle ||
      session->second.sides[0].faction_id.empty() ||
      session->second.sides[1].faction_id.empty() ||
      session->second.sides[0].faction_id ==
          session->second.sides[1].faction_id)
    return fail("FieldDiplomacyOriginalSidesRequired");
  const ArmyRecord *army = nullptr;
  for (const auto &row : x.military.armies)
    if (row.id == army_id) {
      if (army)
        return fail("FieldDiplomacyArmyAmbiguous");
      army = &row;
    }
  if (!army)
    return fail("FieldDiplomacyOriginalArmyRequired");
  auto now = x.actual_clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto formation = read_original_military_formation(
      x.common.state(), x.military, *army, now.value, &x.formations, budget);
  if (!formation.ok)
    return {false, {}, formation.error};
  if (!formation.value.original_terms)
    return fail("FieldDiplomacyOriginalFormationRequired");
  const auto &faction = formation.value.original_terms->organization_id;
  if (before) {
    auto formed = read_actual_common_fact_position(
        x.common, formation.value.completed_source.source_ref);
    auto cut = read_actual_common_fact_position(x.common, *before);
    if (!formed.ok)
      return {false, {}, formed.error};
    if (!cut.ok)
      return {false, {}, cut.error};
    if (formed.value >= cut.value)
      return fail("FieldDiplomacyFormationAfterContact");
  }
  for (Integer side = 0; side != 2; ++side) {
    if (session->second.sides[side].faction_id != faction)
      continue;
    FieldReinforcementDiplomacyRead result;
    result.side = side;
    const auto &original = formation.value.completed_source.source_ref;


    (result.copied_fields).grow(0);
    result.same_faction_formation_source =
        SourceRef{checkpoint_fresh_string_copy(original.domain),
                  checkpoint_fresh_string_copy(original.kind),
                  checkpoint_fresh_string_copy(original.id)};
    return {true, std::move(result), {}};
  }
  std::array<PoliticalRelationOriginalView, 2> relations;
  for (Integer side = 0; side != 2; ++side) {
    auto relation =
        before
            ? read_political_relation_original_view_before_field_source(
                  x, *before, faction, session->second.sides[side].faction_id)
            : read_current_political_relation_original_view(
                  x, faction, session->second.sides[side].faction_id);
    if (!relation.ok)
      return {false, {}, relation.error};
    relations[side] = std::move(relation.value);
  }
  FieldReinforcementDiplomacyRead result;
  const bool first = relations[0].relation == PoliticalRelation::allied;
  const bool second = relations[1].relation == PoliticalRelation::allied;
  if (first != second)
    result.side = first ? 0 : 1;
  CheckpointVector<const FactRecord *> sources;
  for (const auto &relation : relations) {
    if (!relation.initialization ||
        relation.initialization->domain != "politics" ||
        relation.initialization->kind != "political_relations_initialized")
      return fail("FieldDiplomacyActualInitializationRequired");
    auto initial_head =
        read_actual_common_fact_position(x.common, *relation.initialization);
    if (!initial_head.ok)
      return {false, {}, initial_head.error};
    sources.push_back(relation.initialization);
    if (relation.last_change) {
      if (relation.last_change->domain != "politics" ||
          relation.last_change->kind != "political_battle_relation_changed")
        return fail("FieldDiplomacyActualRelationChangeRequired");
      auto changed_head =
          read_actual_common_fact_position(x.common, *relation.last_change);
      if (!changed_head.ok)
        return {false, {}, changed_head.error};
      sources.push_back(relation.last_change);
    }
  }
  CheckpointVector<const FactRecord *> unique;
  for (const auto *source : sources) {
    bool seen = false;
    for (const auto *old : unique)
      if (old == source)
        seen = true;
    if (!seen)
      unique.push_back(source);
  }


  (result.copied_fields).grow(0);
  result.actual_relation_sources.reserve(unique.size());
  for (const auto *source : unique)
    result.actual_relation_sources.push_back(
        {checkpoint_fresh_string_copy(source->domain),
         checkpoint_fresh_string_copy(source->kind),
         checkpoint_fresh_string_copy(source->id)});
  return {true, std::move(result), {}};
}
} // namespace
Outcome<FieldReinforcementDiplomacyRead>
read_current_field_reinforcement_diplomacy(
    const PoliticalRelationsReadContext &x, const StableId &battle,
    const StableId &army, ExactArithmeticBudget budget) {
  return read_alignment(x, battle, army, nullptr, budget);
}
Outcome<FieldReinforcementDiplomacyRead>
read_field_reinforcement_diplomacy_before_contact(
    const PoliticalRelationsReadContext &x, const SourceRef &source,
    ExactArithmeticBudget budget) {
  const auto row = x.field.boundary_contacts.find(source.id);
  if (source.domain != "military" || source.kind != "field_battle_contact" ||
      row == x.field.boundary_contacts.end() ||
      row->second.contact_id != source.id || !row->second.army_id ||
      x.field.contacts.count(source.id))
    return {
        false,
        {},
        common_failure("FieldReinforcementBoundarySourceRequired", source.id)
            .error};
  return read_alignment(x, row->second.battle_id, *row->second.army_id, &source,
                        budget);
}
} // namespace hanmo::resident
