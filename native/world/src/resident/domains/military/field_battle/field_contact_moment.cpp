#include "field_contact_moment.hpp"
#include "field_author_data.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Outcome<FieldInitialContactMomentRead> read_field_initial_contact_actual_moment(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const FieldBattleCatalog &catalog,
    const SourceRef &source, const FieldExactContactReadContext &x,
    ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldInitialContactMomentRead> {
    return {false, {}, common_failure(code, source.id).error};
  };
  if (&x.common != &common || &x.military != &military || &x.field != &world ||
      source.domain != "military" || source.kind != "field_battle_contact")
    return fail("CrossCandidateFieldContactMoment");
  const auto actual = world.contacts.find(source.id);
  if (actual == world.contacts.end() ||
      actual->second.contact_id != source.id ||
      world.boundary_contacts.contains(source.id))
    return fail("FieldInitialContactOriginalSourceRequired");
  auto origin = validate_field_contact_origin_head(common, actual->second);
  if (!origin.ok)
    return {false, {}, origin.error};
  const auto exact =
      std::get_if<FieldExactGroupContactOrigin>(&actual->second.source);
  if (!exact)
    return fail("FieldContactOriginalExactMomentRequired");
  auto read = read_field_exact_road_contact_source(x, exact->actual_contact,
                                                   catalog.parameters, budget);
  if (!read.ok)
    return {false, {}, read.error};
  const auto session = world.sessions.find(actual->second.battle_id);
  if (session == world.sessions.end() ||
      session->second.battle_id != actual->second.battle_id ||
      session->second.created_tick !=
          read.value.contact->occurred_at.half_day_tick)
    return fail("FieldContactOriginalSessionRequired");
  auto here = read_actual_common_fact_position(common, source);
  auto installed = read_actual_common_fact_position(
      common, {"military", "field_battle_session", actual->second.battle_id});
  if (!here.ok)
    return {false, {}, here.error};
  if (!installed.ok)
    return {false, {}, installed.error};
  if (here.value >= installed.value)
    return fail("FieldContactOriginalSourceOrderMismatch");
  return {true,
          {&actual->second, common.state().facts.find(source.id),
           &exact->actual_contact, &read.value.contact->occurred_at},
          {}};
}
} // namespace hanmo::resident
