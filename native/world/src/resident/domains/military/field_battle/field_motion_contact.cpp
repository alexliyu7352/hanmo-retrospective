#include "field_motion_contact.hpp"
#include "field_ability_sources.hpp"
#include "field_author_data.hpp"
#include "field_contact_moment.hpp"
#include "field_install.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/storage/common_store.hpp"
#include <new>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
}
void require(bool value, const char *code) {
  if (!value)
    throw Failure{common_failure(code).error};
}
bool same_source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
// S1 passes a saved value because publication may change its tables. Reborrow
// the unique original row; supplied geometry is never an alternate authority.
const ExactMovementContact &
original_contact(const FieldExactContactReadContext &x,
                 const ExactMovementContact &supplied,
                 ExactArithmeticBudget budget) {
  require(x.clock.bound_to(x.common, x.military, x.field),
          "FieldMotionContactSameWorldRequired");
  const auto *actual = x.movement.contacts.find(supplied.id);
  require(actual && actual->id == supplied.id &&
              actual->first_prefix_id == supplied.first_prefix_id &&
              actual->second_prefix_id == supplied.second_prefix_id &&
              actual->geometry_profile == supplied.geometry_profile &&
              take(compare_world_moments(actual->occurred_at,
                                         supplied.occurred_at, budget)) == 0 &&
              take(compare_world_moments(actual->window_from,
                                         supplied.window_from, budget)) == 0,
          "FieldMotionActualContactRequired");
  const auto *head = x.common.state().facts.find(actual->id);
  require(head && head->id == actual->id && head->domain == "travel" &&
              head->kind == "movement_contact",
          "FieldMotionActualContactHeadRequired");
  return *actual;
}
enum class ContactKind { personal, armies };
ContactKind contact_kind(const FieldExactContactReadContext &x,
                         const ExactMovementContact &c) {
  const auto *first = x.movement.prefixes.find(c.first_prefix_id);
  const auto *second = x.movement.prefixes.find(c.second_prefix_id);
  require(first && second && first != second,
          "FieldMotionContactOriginalPrefixesRequired");
  if (std::holds_alternative<PersonalMovementExecution>(first->execution) &&
      std::holds_alternative<PersonalMovementExecution>(second->execution))
    return ContactKind::personal;
  const auto *a = std::get_if<GroupMovementExecution>(&first->execution);
  const auto *b = std::get_if<GroupMovementExecution>(&second->execution);
  require(a && b && std::holds_alternative<ArmyCarrierRef>(a->carrier) &&
              std::holds_alternative<ArmyCarrierRef>(b->carrier),
          "FieldMotionMixedOrNonarmyContactUnsupported");
  require(std::get<ArmyCarrierRef>(a->carrier).army_id !=
              std::get<ArmyCarrierRef>(b->carrier).army_id,
          "FieldMotionDistinctArmiesRequired");
  return ContactKind::armies;
}
Status validate(const FieldExactContactReadContext &x,
                const FieldBattleCatalog &catalog,
                const MotionContactBusinessRead &personal,
                const ExactMovementContact &supplied,
                const MomentOwnerSource &source, ExactArithmeticBudget budget) {
  try {
    const auto &contact = original_contact(x, supplied, budget);
    if (contact_kind(x, contact) == ContactKind::personal)
      return personal.validate_actual_contact_installation(contact, source);
    require(source.source_schema == 1 &&
                source.source_ref.domain == "military" &&
                source.source_ref.kind == "field_battle_contact",
            "FieldMotionBusinessSourceRequired");
    auto original = take(read_field_initial_contact_actual_moment(
        x.common, x.military, x.field, catalog, source.source_ref, x, budget));
    const SourceRef expected{"travel", "movement_contact", contact.id};
    require(original.original_movement_contact && original.occurred_at &&
                same_source(*original.original_movement_contact, expected) &&
                take(compare_world_moments(*original.occurred_at,
                                           contact.occurred_at, budget)) == 0,
            "FieldMotionBusinessContactMismatch");
    std::size_t count = 0;
    for (const auto &[id, row] : x.field.contacts)
      if (const auto *origin =
              std::get_if<FieldExactGroupContactOrigin>(&row.source);
          origin && same_source(origin->actual_contact, expected)) {
        require(id == row.contact_id && id == source.source_ref.id &&
                    &row == original.field_contact,
                "FieldMotionBusinessContactAmbiguous");
        ++count;
      }
    require(count == 1, "FieldMotionBusinessContactAmbiguous");
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldMotionContactResourceExhausted");
  }
}
} // namespace
NativeFieldMotionContactRead::NativeFieldMotionContactRead(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const NativePersonalMotionContactRead &personal,
    ExactArithmeticBudget budget)
    : exact_(x), catalog_(catalog), personal_(personal), budget_(budget) {}
Status NativeFieldMotionContactRead::validate_actual_contact_installation(
    const ExactMovementContact &contact,
    const MomentOwnerSource &source) const {
  return validate(exact_, catalog_, personal_, contact, source, budget_);
}
NativeFieldMotionContactSources::NativeFieldMotionContactSources(
    FieldMotionContactWriteDependencies x, ExactArithmeticBudget budget)
    : x_(x), budget_(budget) {}
Status NativeFieldMotionContactSources::validate_actual_contact_installation(
    const ExactMovementContact &contact,
    const MomentOwnerSource &source) const {
  return validate(x_.exact, x_.catalog, x_.personal, contact, source, budget_);
}
Outcome<MomentOwnerSource>
NativeFieldMotionContactSources::install_actual_contact_in_candidate(
    const ExactMovementContact &supplied, ExactPhaseWriteContext &capture) {
  try {
    require(&capture == &x_.capture && &x_.common == &x_.exact.common &&
                &x_.field == &x_.exact.field &&
                x_.common.actor_location_capture_identity() &&
                x_.common.actor_condition_capture_identity(),
            "FieldMotionSameCandidateCaptureRequired");
    const auto &contact = original_contact(x_.exact, supplied, budget_);
    require(take(compare_world_moments(
                contact.occurred_at, take(x_.exact.clock.read_current_moment()),
                budget_)) == 0,
            "FieldMotionActualInstallMomentRequired");
    if (contact_kind(x_.exact, contact) == ContactKind::personal)
      return x_.personal.install_actual_contact_in_candidate(contact, capture);
    require(&x_.relations_read.state == &x_.relations &&
                &x_.relations_read.common == &x_.common &&
                &x_.relations_read.field == &x_.field &&
                &x_.relations_read.organizations == &x_.organizations,
            "FieldMotionPoliticalOwnerMismatch");
    const SourceRef movement_source{"travel", "movement_contact", contact.id};
    const auto battle =
        take(install_hostile_field_exact_group_session_in_candidate(
            x_.common, x_.field, x_.exact, x_.relations_read, x_.catalog,
            movement_source, x_.player_actor_id, x_.battle_seed, x_.social,
            x_.organizations, x_.abilities, x_.world_memory));
    const auto session = x_.field.sessions.find(battle);
    require(session != x_.field.sessions.end() &&
                session->second.battle_id == battle,
            "FieldMotionInstalledSessionRequired");
    // A newly Running NPC session already has its genuine start head. Adopt
    // that exact cause before any whole P3 scan, never fabricate a start.
    if (session->second.started_source) {
      const MomentOwnerSource started{*session->second.started_source, 1};
      take(adopt_field_battle_relation_fact_in_candidate(
          x_.common, x_.relations, x_.relations_read, started));
    } else
      require(session->second.state != FieldBattleState::Running,
              "FieldMotionActualStartRequired");
    const FieldBattleContactRecord *found = nullptr;
    for (const auto &[id, row] : x_.field.contacts)
      if (const auto *origin =
              std::get_if<FieldExactGroupContactOrigin>(&row.source);
          origin && same_source(origin->actual_contact, movement_source)) {
        require(!found && row.battle_id == battle && row.contact_id == id,
                "FieldMotionInstalledContactAmbiguous");
        found = &row;
      }
    require(found, "FieldMotionInstalledContactRequired");
    MomentOwnerSource result{
        {"military", "field_battle_contact", found->contact_id}, 1};
    take(
        validate(x_.exact, x_.catalog, x_.personal, supplied, result, budget_));
    return {true, std::move(result), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {
        false, {}, common_failure("FieldMotionContactResourceExhausted").error};
  }
}
} // namespace hanmo::resident
