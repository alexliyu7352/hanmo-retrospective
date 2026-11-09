#include "field_reinforcement_prepare.hpp"
#include "field_author_data.hpp"
#include "field_reinforcement_moment.hpp"
#include "field_diplomacy_source.hpp"
#include "field_formations.hpp"
#include "field_segment_settlement.hpp"
#include "field_world.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/travel/movement_boundary_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
Outcome<FieldReinforcementPreparation>
prepare_field_reinforcement_at_original_entry(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const SourceRef &contact,
    ExactArithmeticBudget b) {
  return guard<FieldReinforcementPreparation>([&] {
    if (contact.domain != "military" ||
        contact.kind != "field_battle_contact" ||
        &relations.common != &x.common || &relations.field != &x.field ||
        &relations.military != &x.military ||
        &relations.formations != &x.formations ||
        !x.clock.bound_to(x.common, x.military, x.field))
      fail("FieldReinforcementOriginalContextRequired", contact.id);
    const auto found = x.field.boundary_contacts.find(contact.id);
    if (found == x.field.boundary_contacts.end())
      fail("FieldReinforcementActualContactRequired", contact.id);
    const auto &boundary = found->second;
    const auto *origin =
        std::get_if<FieldExactBoundaryOrigin>(&boundary.motion);
    if (!origin || boundary.contact_id != contact.id ||
        boundary.status != FieldBoundaryStatus::Stopped || !boundary.side ||
        !boundary.army_id || boundary.entry_application_source ||
        boundary.entered_actual_at || boundary.entered_at ||
        boundary.exact_continuation_source)
      fail("FieldReinforcementPendingAdmissionRequired", contact.id);
    FieldReinforcementPreparation out;
    out.boundary = &boundary;
    out.original = take(read_field_boundary_original_source(
        x, catalog, origin->actual_contact, b));
    out.actual_at = take(x.clock.read_current_moment());
    if (!out.original.army || *boundary.army_id != out.original.army->id ||
        boundary.battle_id != out.original.contact->original_field_source.id ||
        boundary.entry_point_dm.x != out.original.entry_point_dm.x ||
        boundary.entry_point_dm.y != out.original.entry_point_dm.y ||
        boundary.participant_actor_ids !=
            out.original.basis->actual_participants)
      fail("FieldReinforcementActualEntryMismatch", contact.id);
    const auto cut = take(read_actual_common_fact_position(x.common, contact));
    if (take(read_actual_common_fact_position(
            x.common, SourceRef{"travel", "movement_unit_close",
                                out.original.closed->id})) >= cut)
      fail("FieldReinforcementOriginalCloseRequired", contact.id);
    const auto relation =
        take(read_field_reinforcement_diplomacy_before_contact(relations,
                                                               contact, b));
    if (relation.side != boundary.side ||
        boundary.diplomacy_sources.size() !=
            relation.actual_relation_sources.size() +
                (relation.same_faction_formation_source ? 1 : 0))
      fail("FieldReinforcementActualSideRequired", contact.id);
    std::size_t pin = 0;
    for (const auto &source : relation.actual_relation_sources)
      if (!same_source(boundary.diplomacy_sources[pin++], source))
        fail("FieldReinforcementActualRelationSourceRequired", contact.id);
    if (relation.same_faction_formation_source &&
        !same_source(boundary.diplomacy_sources[pin],
                     *relation.same_faction_formation_source))
      fail("FieldReinforcementActualFormationSourceRequired", contact.id);
    const auto session = x.field.sessions.find(boundary.battle_id);
    if (session == x.field.sessions.end() ||
        session->second.state != FieldBattleState::Running ||
        session->second.result || *boundary.side < 0 || *boundary.side > 1)
      fail("FieldReinforcementRunningSessionRequired", contact.id);
    out.session = &session->second;
    for (const auto &pending : out.session->pending_entries)
      if (pending.contact_id == contact.id) {
        if (out.pending)
          fail("FieldReinforcementPendingEntryAmbiguous", contact.id);
        out.pending = &pending;
      }
    if (!out.pending || out.pending->army_id != *boundary.army_id ||
        out.pending->side != *boundary.side ||
        out.pending->entry.tick != out.original.entry.tick ||
        out.pending->entry.t != out.original.entry.t ||
        out.pending->entry_point_dm.x != out.original.entry_point_dm.x ||
        out.pending->entry_point_dm.y != out.original.entry_point_dm.y ||
        out.session->now.tick != out.original.entry.tick ||
        out.session->now.t != out.original.entry.t)
      fail("FieldReinforcementExactEntryBoundaryRequired", contact.id);
    // S1 发生时刻保原源；局部 entry 在宏观提交后应用，借真实 closing 准入。
    take(validate_field_reinforcement_application_time(
        x, out.original, out.session->now, out.actual_at, catalog.parameters,
        true, b));
    out.original_formations = take(read_field_army_formations_at_original_clock(
        x.common, x.military, x.military_catalog, x.field, *boundary.army_id,
        *boundary.side, x.common.state().metadata.player_actor_id, x.clock,
        x.formations, &out.copied_formations));
    if (out.original_formations.empty())
      fail("FieldReinforcementActualFormationsRequired", contact.id);
    // 31ef: this is the actual queue order, not initial field placement.
    // The commander's own formation is last even when its troop type is foot.
    std::size_t commanders = 0;
    for (const auto &formation : out.original_formations)
      if (formation.commander_actor_id == out.original.army->commander_actor_id)
        ++commanders;
    if (commanders != 1)
      fail("FieldReinforcementOriginalCommanderFormationRequired", contact.id);
    const auto rank = [&](const FieldFormation &formation) {
      if (formation.commander_actor_id == out.original.army->commander_actor_id)
        return 3;
      if (formation.category == FieldTroopCategory::Infantry)
        return 0;
      if (formation.category == FieldTroopCategory::Archer)
        return 1;
      if (formation.category == FieldTroopCategory::Cavalry)
        return 2;
      fail("InvalidFieldTroopCategory", formation.formation_id);
    };
    std::sort(out.original_formations.begin(), out.original_formations.end(),
              [&](const auto &a, const auto &b) {
                const auto first = rank(a), second = rank(b);
                return first != second ? first < second
                                       : a.formation_id < b.formation_id;
              });
    std::size_t count = 0;
    for (const auto &[id, formation] : out.session->formations) {
      if (formation.side == *boundary.side)
        ++count;
      if (formation.army_id == *boundary.army_id)
        fail("FieldReinforcementArmyAlreadyEntered", contact.id);
      for (const auto &arriving : out.original_formations)
        if (arriving.formation_id == id)
          fail("FieldReinforcementFormationAlreadyEntered", id);
    }
    if (count > 32 || out.original_formations.size() > 32 - count)
      fail("FieldReinforcementFormationLimit", contact.id);
    return out;
  });
}
} // namespace hanmo::resident
