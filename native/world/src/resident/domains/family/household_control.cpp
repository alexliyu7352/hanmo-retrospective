#include "resident/kernel/catalog_section_observation.hpp"
#include "household_control.hpp"
#include "family_model.hpp"
#include "life_age.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
bool operation_allowed(const HouseholdControlCatalog &c,
                       const StableId &operation) {
  return operation == household_provision_operation ||
         (c.purchase_control_enabled &&
          operation == household_purchase_operation) ||
         (c.market_staffing_control_enabled &&
          (operation == household_market_staffing_operation ||
           operation == household_market_staffing_release_operation));
}
bool id_ok(const StableId &id) { return !id.empty() && id.size() <= 256; }
bool fact_id_ok(const StableId &id, Integer next) {
  constexpr char prefix[] = "household_control:";
  constexpr std::size_t width = sizeof(prefix) - 1;
  if (id.rfind(prefix, 0) != 0)
    return false;
  const auto suffix = id.substr(width);
  Integer number = 0;
  const auto parsed =
      std::from_chars(suffix.data(), suffix.data() + suffix.size(), number);
  return parsed.ec == std::errc{} &&
         parsed.ptr == suffix.data() + suffix.size() && number > 0 &&
         number < next && std::to_string(number) == suffix;
}
bool household_assets(const CommonStore &common,
                      const LocalEconomyState &economy, const StableId &id) {
  const auto *h = economy.households.find(id);
  if (!h)
    return false;
  const auto *wallet = common.wallet(h->wallet_id);
  const auto *inventory = common.inventory(h->inventory_id);
  return wallet && inventory && wallet->owner.kind == Owner::Kind::household &&
         inventory->owner.kind == Owner::Kind::household &&
         wallet->owner.id == id && inventory->owner.id == id;
}
// The old profile consumes its actual initial birth. Origin2's declared age is
// not a birthday: resolve its immutable original initial source, never today's
// age.
Outcome<bool> initial_adult(const CommonStore &common,
                            const CommonCatalog &shared, const StableId &actor,
                            const LifeAgeSources *ages) {
  const auto *birth = shared.actor_initial_birth.find(actor);
  if (!birth)
    return {true, false, {}};
  const bool authored =
      ages && ages->foundation.actor_start_context.actor_age_bases.find(actor);
  if (birth->has_value() && !authored)
    return {true, **birth <= -(18 * 720), {}};
  if (birth->has_value())
    return {false,
            {},
            common_failure("HouseholdDeclaredAgeIsNotBirthday", actor).error};
  if (!ages || &ages->common != &common || &ages->control.common != &common ||
      &ages->control.shared != &shared)
    return {false,
            {},
            common_failure("HouseholdOrigin2AgeSourcesRequired", actor).error};
  const auto *origin =
      ages->foundation.character_origin_records.origins.find(actor);
  if (!origin ||
      origin->origin_kind != CharacterOriginRecordV2::Kind::authored ||
      origin->created_at.kind != ActorFoundationAt::Kind::exact_moment)
    return {false,
            {},
            common_failure("HouseholdAuthoredInitialAgeRequired", actor).error};
  // The original Life reader authenticates this same Origin2 before reading
  // its declared age; no current-age-to-birthday inversion is performed.
  auto age = read_life_age_basis(*ages, actor, origin->created_at.exact_moment);
  if (!age.ok)
    return {false, {}, age.error};
  const auto *declared = std::get_if<DeclaredAgeBasisRef>(&age.value.ref);
  if (!declared || declared->kind != DeclaredAgeBasisRef::Kind::actor_start ||
      declared->actor_id != actor || declared->source_ref != actor)
    return {false,
            {},
            common_failure("HouseholdOrigin2InitialAgeMismatch", actor).error};
  return {true, age.value.age_years >= 18, {}};
}
bool matches(const HouseholdControlFact &f,
             const HouseholdControlDefinition &d) {
  return f.definition_id == d.id && f.actor_id == d.actor_id &&
         f.household_group_id == d.household_group_id &&
         f.source_role_id == d.source_role_id && f.operation == d.operation &&
         f.effective_from_tick == d.effective_from_tick &&
         f.through_tick == d.through_tick &&
         f.actor_accepted == d.actor_accepted;
}
} // namespace
Status validate_household_control_catalog(
    const CommonStore &common, const CommonCatalog &shared,
    const LocalEconomyState &economy, const HouseholdControlCatalog &catalog,
    const LifeAgeSources *initial_age) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 35, "HouseholdControlCatalog", "validate_household_control_catalog", &catalog);

  if (catalog.initial_mandates.empty() ||
      catalog.initial_mandates.size() > 4096)
    return common_failure("InvalidHouseholdControlCatalog");
  std::set<std::pair<StableId, StableId>> household_operations;
  for (const auto &row : catalog.initial_mandates.rows()) {
    const auto &d = row.second;
    const auto *actor = common.state().actors.find(d.actor_id);
    const auto *role = shared.roles.find(d.source_role_id);
    if (row.first != d.id || !id_ok(d.id) || !id_ok(d.actor_id) ||
        !id_ok(d.household_group_id) || !id_ok(d.source_role_id) ||
        !d.actor_accepted || !operation_allowed(catalog, d.operation) ||
        d.effective_from_tick != 0 ||
        (d.through_tick &&
         (*d.through_tick < 0 || *d.through_tick > 2147483647)))
      return common_failure("InvalidHouseholdControlDefinition", row.first);
    // 初态承认的成年人和私务岗位是明确来源，公职/聚合户资产owner本身均不授此权。
    if (!actor || !role || role->initial_actor != d.actor_id ||
        role->organization_id || !shared.places.find(role->place_id))
      return common_failure("InvalidHouseholdControlSubject", row.first);
    auto adult = initial_adult(common, shared, d.actor_id, initial_age);
    if (!adult.ok)
      return {false, {}, adult.error};
    if (!adult.value)
      return common_failure("InvalidHouseholdControlSubject", row.first);
    if (!household_assets(common, economy, d.household_group_id))
      return common_failure("InvalidHouseholdControlAssets", row.first);
    // 每户每用途只承认一份明确委托；新增采购用途不能继承供养权或出现竞争受托人。
    if (!household_operations
             .insert({d.household_group_id,
                      (catalog.purchase_control_enabled ||
                       catalog.market_staffing_control_enabled)
                          ? d.operation
                          : StableId{}})
             .second)
      return common_failure("DuplicateHouseholdControl", d.household_group_id);
  }
  return common_success();
}
Status validate_household_control(const CommonStore &common,
                                  const FamilyState &family,
                                  const CommonCatalog &shared,
                                  const LocalEconomyState &economy,
                                  const HouseholdControlCatalog *catalog,
                                  const LifeAgeSources *initial_age) {
  if (catalog) {
    auto valid = validate_household_control_catalog(common, shared, economy,
                                                    *catalog, initial_age);
    if (!valid.ok)
      return valid;
  }
  std::set<StableId> definitions;
  for (const auto &row : family.facts.rows()) {
    const auto *f = std::get_if<HouseholdControlFact>(&row.second);
    if (!f)
      continue;
    if (!catalog)
      return common_failure("HouseholdControlCapabilityRequired", row.first);
    const auto *definition = catalog->initial_mandates.find(f->definition_id);
    const auto *header = common.state().facts.find(row.first);
    if (!fact_id_ok(row.first, common.state().next_instance_id) ||
        !definition || !header || header->domain != "family" ||
        header->kind != "family_household_control" || f->kind != header->kind ||
        f->recorded_tick != 0 || f->recorded_tick > common.tick() ||
        f->scenario_id != common.state().metadata.scenario_id ||
        !matches(*f, *definition) ||
        !definitions.insert(f->definition_id).second)
      return common_failure("InvalidHouseholdControlFact", row.first);
  }
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "family" &&
        row.second.kind == "family_household_control") {
      const auto *f = family.facts.find(row.first);
      if (!f || !std::holds_alternative<HouseholdControlFact>(*f))
        return common_failure("MissingHouseholdControlFact", row.first);
    }
  if (catalog && definitions.size() != catalog->initial_mandates.size())
    return common_failure("MissingHouseholdControlSource");
  return common_success();
}
Status initialize_household_control(CommonStore &common, FamilyState &family,
                                    const CommonCatalog &shared,
                                    const LocalEconomyState &economy,
                                    const HouseholdControlCatalog &catalog,
                                    const LifeAgeSources *initial_age) {
  if (common.tick() != 0 || common.state().metadata.scenario_id.empty())
    return common_failure("HouseholdControlInitialBoundaryRequired");
  auto valid = validate_household_control_catalog(common, shared, economy,
                                                  catalog, initial_age);
  if (!valid.ok)
    return valid;
  for (const auto &row : family.facts.rows())
    if (std::holds_alternative<HouseholdControlFact>(row.second))
      return common_failure("HouseholdControlAlreadyInitialized");
  for (const auto &row : catalog.initial_mandates.rows()) {
    const auto &d = row.second;
    const auto *role = common.state().roles.find(d.source_role_id);
    const auto *actor = common.actor(d.actor_id);
    if (!role || !role->enabled || role->actor_id != d.actor_id || !actor ||
        !actor->alive)
      return common_failure("HouseholdControlInitialRoleRequired", row.first);
  }
  auto point = common.begin_savepoint();
  if (!point.ok)
    return {false, {}, point.error};
  auto candidate = family;
  auto abort = [&](Status error) {
    auto rolled = common.rollback_savepoint(point.value);
    return rolled.ok ? error : rolled;
  };
  for (const auto &row : catalog.initial_mandates.rows()) {
    const auto &d = row.second;
    auto id = common.next_id("household_control");
    if (!id.ok)
      return abort({false, {}, id.error});
    auto head =
        common.append_fact({id.value, "family_household_control", "family"});
    if (!head.ok)
      return abort(head);
    HouseholdControlFact fact;
    fact.definition_id = d.id;
    fact.actor_id = d.actor_id;
    fact.household_group_id = d.household_group_id;
    fact.source_role_id = d.source_role_id;
    fact.operation = d.operation;
    fact.scenario_id = common.state().metadata.scenario_id;
    fact.effective_from_tick = d.effective_from_tick;
    fact.through_tick = d.through_tick;
    fact.actor_accepted = d.actor_accepted;
    if (!candidate.facts.insert(id.value, std::move(fact)))
      return abort(common_failure("DuplicateFamilyFact", id.value));
  }
  valid = validate_household_control(common, candidate, shared, economy,
                                     &catalog, initial_age);
  if (!valid.ok)
    return abort(valid);
  auto committed = common.commit_savepoint(point.value);
  if (!committed.ok)
    return committed;
  family = std::move(candidate);
  return common_success();
}
Outcome<std::optional<HouseholdProvisionControlProof>>
household_provision_control_for_actor(
    const CommonStore &common, const FamilyState &family,
    const FamilyCatalog &catalog, const CommonCatalog &shared,
    const LocalEconomyState &economy, const StableId &actor_id,
    const StableId &household_id, const StableId &operation, Integer tick,
    const LifeAgeSources *initial_age) {
  using Result = Outcome<std::optional<HouseholdProvisionControlProof>>;
  const auto fail = [](Status s) -> Result { return {false, {}, s.error}; };
  if (!catalog.household_control)
    return fail(common_failure("HouseholdControlUnavailable", {},
                               ErrorKind::unsupported_capability));
  const auto *actor = common.actor(actor_id);
  if (!actor)
    return fail(common_failure("UnknownActor", actor_id));
  if (!economy.households.find(household_id))
    return fail(common_failure("UnknownHousehold", household_id));
  if (tick != common.tick())
    return fail(common_failure("HouseholdControlCurrentTickRequired"));
  if (!operation_allowed(*catalog.household_control, operation))
    return fail(common_failure("UnsupportedHouseholdControlOperation",
                               operation, ErrorKind::unsupported_capability));
  auto valid =
      validate_household_control(common, family, shared, economy,
                                 &*catalog.household_control, initial_age);
  if (!valid.ok)
    return fail(valid);
  if (!actor->alive)
    return {true, std::nullopt, {}};
  for (const auto &row : family.facts.rows()) {
    const auto *fact = std::get_if<HouseholdControlFact>(&row.second);
    if (!fact || fact->actor_id != actor_id ||
        fact->household_group_id != household_id ||
        fact->operation != operation)
      continue;
    if (tick < fact->effective_from_tick ||
        (fact->through_tick && tick > *fact->through_tick))
      return {true, std::nullopt, {}};
    const auto *role = common.state().roles.find(fact->source_role_id);
    if (!role)
      return fail(
          common_failure("UnknownHouseholdControlRole", fact->source_role_id));
    if (!role->enabled || role->actor_id != actor_id)
      return {true, std::nullopt, {}};
    return {true,
            HouseholdProvisionControlProof{
                actor_id, household_id, row.first, fact->source_role_id,
                operation, common.revision(), fact->effective_from_tick,
                fact->through_tick},
            {}};
  }
  return {true, std::nullopt, {}};
}
Status validate_household_provision_control_source(
    const CommonStore &common, const FamilyState &family,
    const FamilyCatalog &catalog, const CommonCatalog &shared,
    const LocalEconomyState &economy,
    const HouseholdProvisionControlProof &proof, Integer accepted_tick,
    const LifeAgeSources *initial_age) {
  if (!catalog.household_control)
    return common_failure("HouseholdControlUnavailable", {},
                          ErrorKind::unsupported_capability);
  auto valid =
      validate_household_control(common, family, shared, economy,
                                 &*catalog.household_control, initial_age);
  if (!valid.ok)
    return valid;
  const auto *value = family.facts.find(proof.fact_id);
  const auto *fact = value ? std::get_if<HouseholdControlFact>(value) : nullptr;
  if (!fact || accepted_tick < fact->recorded_tick ||
      accepted_tick < fact->effective_from_tick ||
      accepted_tick > common.tick() ||
      (fact->through_tick && accepted_tick > *fact->through_tick) ||
      proof.source_revision < 0 || proof.source_revision > common.revision())
    return common_failure("InvalidAcceptedHouseholdControlSource",
                          proof.fact_id);
  if (proof.actor_id != fact->actor_id ||
      proof.household_group_id != fact->household_group_id ||
      proof.source_role_id != fact->source_role_id ||
      proof.operation != fact->operation ||
      proof.effective_from_tick != fact->effective_from_tick ||
      proof.through_tick != fact->through_tick)
    return common_failure("HouseholdControlProofMismatch", proof.fact_id);
  return common_success();
}
FamilyHouseholdControlAccess::FamilyHouseholdControlAccess(
    const CommonStore &c, const FamilyState &f, const FamilyCatalog &d,
    const CommonCatalog &shared, const LocalEconomyState &e,
    const LifeAgeSources *initial_age)
    : common_(c), family_(f), catalog_(d), shared_(shared), economy_(e),
      initial_age_(initial_age) {}
Outcome<std::optional<HouseholdProvisionControlProof>>
FamilyHouseholdControlAccess::current(const StableId &actor,
                                      const StableId &household,
                                      const StableId &operation,
                                      Integer tick) const {
  return household_provision_control_for_actor(
      common_, family_, catalog_, shared_, economy_, actor, household,
      operation, tick, initial_age_);
}
Status FamilyHouseholdControlAccess::validate_accepted(
    const HouseholdProvisionControlProof &proof, Integer tick) const {
  return validate_household_provision_control_source(
      common_, family_, catalog_, shared_, economy_, proof, tick, initial_age_);
}
// 共用原初态受托、真实 Common 头及唯一 Family 事实的不可变来源门。
Outcome<const HouseholdControlFact *> read_original_household_steward_source(
    const CommonStore &common, const FamilyState &family,
    const FamilyCatalog &catalog, const CommonCatalog &shared,
    const SourceRef &source, const LifeAgeSources *ages) {
  const auto reject =
      [&](const char *code) -> Outcome<const HouseholdControlFact *> {
    return {false, nullptr, common_failure(code, source.id).error};
  };
  if (source.domain != "family" || source.kind != "family_household_control" ||
      !catalog.household_control ||
      !fact_id_ok(source.id, common.state().next_instance_id))
    return reject("HouseholdStewardOriginalSourceRequired");
  if (ages && &ages->family != &family)
    return reject("HouseholdStewardSameFamilyRootRequired");
  const auto *body = family.facts.find(source.id);
  const auto *fact = body ? std::get_if<HouseholdControlFact>(body) : nullptr;
  const auto *head = common.state().facts.find(source.id);
  const auto *def = fact ? catalog.household_control->initial_mandates.find(
                               fact->definition_id)
                         : nullptr;
  if (!fact || !head || head->domain != source.domain ||
      head->kind != source.kind || fact->kind != source.kind || !def ||
      def->id != fact->definition_id || !matches(*fact, *def) ||
      !fact->actor_accepted || fact->recorded_tick != 0 ||
      fact->effective_from_tick != 0 ||
      fact->scenario_id != common.state().metadata.scenario_id ||
      !operation_allowed(*catalog.household_control, fact->operation) ||
      (fact->through_tick &&
       (*fact->through_tick < 0 || *fact->through_tick > 2147483647)))
    return reject("InvalidHouseholdStewardOriginalSource");
  const auto *role = shared.roles.find(fact->source_role_id);
  if (!id_ok(def->id) || !id_ok(def->actor_id) ||
      !id_ok(def->household_group_id) || !id_ok(def->source_role_id) ||
      !common.actor(def->actor_id) || !role ||
      role->initial_actor != def->actor_id || role->organization_id ||
      !shared.places.find(role->place_id))
    return reject("InvalidHouseholdStewardOriginalSubject");
  auto adult = initial_adult(common, shared, def->actor_id, ages);
  if (!adult.ok)
    return {false, nullptr, adult.error};
  if (!adult.value)
    return reject("HouseholdStewardOriginalAdultRequired");
  std::size_t count = 0;
  for (const auto &row : family.facts.rows()) {
    const auto *other = std::get_if<HouseholdControlFact>(&row.second);
    if (other && other->definition_id == def->id)
      ++count;
  }
  if (count != 1)
    return reject("DuplicateHouseholdStewardOriginalSource");
  return {true, fact, {}};
}
} // namespace hanmo::resident
