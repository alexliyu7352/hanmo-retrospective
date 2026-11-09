#include "resident/domains/military/military_model.hpp"
// 新控制revision由真实登记的因果顺序派生，任命签发历史按原事实边界读取。
#include "politics_takeover_catalog.hpp"
#include "politics_takeover_territory.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/field_battle/field_contact_moment.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/original_catalog_identity.hpp"

#include "politics_takeover_detail.hpp"
#include "resident/domains/military/named_military_custody_history_model.hpp"
#include "resident/domains/military/occupation_applied_source.hpp"
#include "resident/domains/politics/budget/political_budget_contract.hpp"
#include "resident/domains/politics/governance_targets/governance_program.hpp"
#include "resident/domains/politics/head_handover/political_head_handover_time2.hpp"
#include "resident/domains/politics/politics_time2.hpp"
namespace hanmo::resident {
Status apply_political_takeover_control_in_candidate(
    const CommonStore &common, PoliticsState &politics,
    const PoliticalTakeoverCatalog &catalog,
    const PoliticalTakeoverState &state, const OccupationAppliedSource &applied,
    const StableId &registration_id) {
  using namespace takeover_detail;
  const auto *registration = state.registrations.find(registration_id);
  const auto *rule =
      registration ? catalog.rules.find(registration->rule_id) : nullptr;
  auto *jurisdiction =
      rule ? politics.jurisdictions.find(rule->jurisdiction_id) : nullptr;
  if (!catalog.enabled || (catalog.version != 1 && catalog.version != 2) ||
      state.version != catalog.version || !registration || !rule ||
      !jurisdiction || !same_identity(common.state().identity, applied.world) ||
      registration->occupation_fact_id != applied.occupation_id ||
      registration->tick != applied.effective_tick ||
      registration->former_organization_id != applied.from_organization_id ||
      registration->successor_organization_id != applied.to_organization_id ||
      !header(common, registration_id, political_takeover_pending_fact_kind))
    return common_failure("InvalidPoliticalControlApplication",
                          registration_id);
  Integer count = 0;
  for (const auto &row : state.registrations.rows()) {
    const auto *selected = catalog.rules.find(row.second.rule_id);
    if (selected && selected->jurisdiction_id == jurisdiction->id) {
      if (count == INT64_MAX)
        return common_failure("PoliticalControlRevisionOverflow");
      ++count;
    }
  }
  if (count <= 0 || jurisdiction->revision != count - 1 ||
      jurisdiction->controlling_organization_id !=
          applied.from_organization_id ||
      jurisdiction->seat_place_id != rule->site_id)
    return common_failure("PoliticalControlBoundaryChanged", jurisdiction->id);
  jurisdiction->controlling_organization_id = applied.to_organization_id;
  jurisdiction->revision = count;
  return common_success();
}
Outcome<std::optional<PoliticalTerritoryControl>>
PoliticalTakeoverTerritoryAccess::jurisdiction_control(
    const StableId &id) const {
  const auto &context = context_;
  const auto *initial = context.politics_catalog.jurisdictions.find(id);
  if (!initial)
    return {
        false, {}, common_failure("UnknownPoliticalJurisdiction", id).error};
  const PoliticalTakeoverRule *rule = nullptr;
  for (const auto &row : context.catalog.rules.rows())
    if (row.second.jurisdiction_id == id) {
      rule = &row.second;
      break;
    }
  if (!context.catalog.enabled || !rule)
    return {true, std::nullopt, {}};
  auto valid = validate_political_takeover_state(context);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto current = military_garrison_owner(
      context.common, context.military, context.military_catalog,
      context.military_access, rule->garrison_id);
  if (!current.ok)
    return {false, {}, current.error};
  PoliticalTerritoryControl out;
  out.source_identity = context.common.state().identity;
  out.jurisdiction_id = id;
  out.controlling_organization_id = current.value.organization_id;
  out.occupation_fact_id = current.value.occupation_id;
  const PoliticalTakeoverRegistration *last = nullptr;
  Integer sequence = 0;
  for (const auto &row : context.state.registrations.rows()) {
    const auto *selected = context.catalog.rules.find(row.second.rule_id);
    if (!selected || selected->jurisdiction_id != id)
      continue;
    auto number =
        organization_instance_sequence(row.first, "political_takeover_pending");
    if (!number || out.revision == INT64_MAX)
      return {false,
              {},
              common_failure("InvalidPoliticalControlSource", row.first).error};
    ++out.revision;
    if (*number > sequence) {
      sequence = *number;
      last = &row.second;
    }
  }
  if (last
          ? (last->occupation_fact_id != out.occupation_fact_id ||
             last->successor_organization_id != out.controlling_organization_id)
          : (!out.occupation_fact_id.empty() ||
             out.controlling_organization_id !=
                 initial->controlling_organization_id))
    return {
        false, {}, common_failure("PoliticalControlSourceMismatch", id).error};
  return {true, std::move(out), {}};
}
Outcome<Integer>
PoliticalTakeoverTerritoryAccess::jurisdiction_revision_before_fact(
    const StableId &id, const StableId &fact_id, Integer tick) const {
  const auto &context = context_;
  auto current = jurisdiction_control(id);
  if (!current.ok)
    return {false, 0, current.error};
  if (context.catalog.version == 2) {
    if (!context.politics_state || !context.organization_runtime ||
        !context.actual_clock)
      return {false, 0,
              common_failure("PoliticalControlTime2SourcesRequired", fact_id)
                  .error};
    auto at = read_politics_source_moment(
        context.common, *context.politics_state, *context.organization_runtime,
        {"politics", "political_appointment_offered", fact_id}, 2,
        *context.actual_clock);
    if (!at.ok)
      return {false, 0, at.error};
    if (at.value.half_day_tick != tick)
      return {
          false, 0,
          common_failure("PoliticalControlTime2TickMismatch", fact_id).error};
    const auto &order = context.common.state().facts.order();
    auto cut = std::find(order.begin(), order.end(), fact_id);
    if (cut == order.end())
      return {false, 0,
              common_failure("PoliticalControlSourceMissing", fact_id).error};
    Integer revision = 0;
    for (const auto &row : context.state.registrations.rows()) {
      const auto *rule = context.catalog.rules.find(row.second.rule_id);
      if (!rule || rule->jurisdiction_id != id)
        continue;
      auto source = std::find(order.begin(), order.end(), row.first);
      if (source == order.end())
        return {
            false, 0,
            common_failure("PoliticalControlSourceMissing", row.first).error};
      if (source >= cut)
        continue;
      if (!row.second.registered_at)
        return {false, 0,
                common_failure("PoliticalTakeoverRegistrationMomentRequired",
                               row.first)
                    .error};
      auto compare = compare_world_moments(*row.second.registered_at, at.value);
      if (!compare.ok)
        return {false, 0, compare.error};
      if (compare.value > 0 || revision == INT64_MAX)
        return {
            false, 0,
            common_failure("InvalidPoliticalControlCausality", fact_id).error};
      ++revision;
    }
    return {true, revision, {}};
  }
  const auto *head = context.common.state().facts.find(fact_id);
  auto sequence = organization_instance_sequence(fact_id, "politics");
  if (!head || head->domain != "politics" ||
      head->kind != "political_appointment_offered" || !sequence ||
      *sequence >= context.common.state().next_instance_id || tick < 0 ||
      tick > context.common.tick())
    return {false, 0,
            common_failure("InvalidPoliticalAppointmentControlSource", fact_id)
                .error};
  Integer revision = 0;
  for (const auto &row : context.state.registrations.rows()) {
    const auto *rule = context.catalog.rules.find(row.second.rule_id);
    if (!rule || rule->jurisdiction_id != id)
      continue;
    auto number =
        organization_instance_sequence(row.first, "political_takeover_pending");
    if (!number)
      return {false, 0,
              common_failure("InvalidPoliticalControlSource", row.first).error};
    if (*number < *sequence) {
      if (row.second.tick > tick || revision == INT64_MAX)
        return {
            false, 0,
            common_failure("InvalidPoliticalControlCausality", fact_id).error};
      ++revision;
    }
  }
  return {true, revision, {}};
}
Outcome<PoliticalTerritoryControl>
PoliticalTakeoverTerritoryAccess::governance_jurisdiction_control_current(
    const StableId &id) const {
  const auto &x = context_;
  const auto *seed = x.politics_catalog.jurisdictions.find(id);
  if (!seed || seed->id != id || seed->revision != 0 ||
      seed->controlling_organization_id.empty())
    return {false,
            {},
            common_failure("GovernanceJurisdictionOriginalMissing", id).error};
  auto sources = validate_political_takeover_state(x);
  if (!sources.ok)
    return {false, {}, sources.error};
  auto actual = jurisdiction_control(id);
  if (!actual.ok)
    return {false, {}, actual.error};
  PoliticalTerritoryControl result = actual.value.value_or(
      PoliticalTerritoryControl{x.common.state().identity,
                                id,
                                seed->controlling_organization_id,
                                {},
                                seed->revision});
  if (!x.politics_state)
    return {
        false,
        {},
        common_failure("GovernanceJurisdictionCurrentBodyRequired", id).error};
  const auto *current = x.politics_state->jurisdictions.find(id);
  if (!current || current->revision != result.revision ||
      current->controlling_organization_id !=
          result.controlling_organization_id ||
      current->political_organization_id != seed->political_organization_id ||
      current->office_id != seed->office_id ||
      current->seat_place_id != seed->seat_place_id ||
      current->public_wallet_id != seed->public_wallet_id ||
      current->public_inventory_ids != seed->public_inventory_ids)
    return {false,
            {},
            common_failure("GovernanceJurisdictionCurrentMismatch", id).error};
  return {true, std::move(result), {}};
}
Outcome<const StableId *>
PoliticalTakeoverTerritoryAccess::military_settlement_jurisdiction(
    const StableId &id) const {
  const auto &x = context_;
  if (!x.politics_catalog.enabled || id.empty())
    return {false, nullptr,
            common_failure("MilitarySettlementJurisdictionOriginalRequired", id,
                           ErrorKind::unsupported_capability)
                .error};
  const StableId *selected = nullptr;
  for (const auto &row : x.politics_catalog.jurisdictions.rows()) {
    const auto &jurisdiction = row.second;
    const auto *seat = x.common_catalog.places.find(jurisdiction.seat_place_id);
    if (row.first != jurisdiction.id || jurisdiction.id.empty() ||
        jurisdiction.revision != 0 || !seat ||
        seat->id != jurisdiction.seat_place_id || seat->settlement_id.empty())
      return {false, nullptr,
              common_failure("MilitarySettlementJurisdictionSourceMismatch",
                             row.first)
                  .error};
    if (seat->settlement_id != id)
      continue;
    if (selected)
      return {
          false, nullptr,
          common_failure("MilitarySettlementJurisdictionAmbiguous", id).error};
    selected = &jurisdiction.id;
  }
  if (!selected)
    return {false, nullptr,
            common_failure("MilitarySettlementJurisdictionOriginalRequired", id,
                           ErrorKind::unsupported_capability)
                .error};
  return {true, selected, {}};
}
Outcome<const StableId *>
PoliticalTakeoverTerritoryAccess::military_stronghold_jurisdiction(
    const StableId &id) const {
  const auto &x = context_;
  auto valid = validate_political_takeover_catalog(
      x.common_catalog, x.organization_catalog, x.politics_catalog,
      x.military_catalog, x.catalog);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  const PoliticalTakeoverRule *selected = nullptr;
  for (const auto &row : x.catalog.rules.rows()) {
    const auto &rule = row.second;
    if (rule.stronghold_id != id)
      continue;
    if (selected && (selected->jurisdiction_id != rule.jurisdiction_id ||
                     selected->site_id != rule.site_id ||
                     selected->garrison_id != rule.garrison_id))
      return {
          false, nullptr,
          common_failure("MilitaryStrongholdJurisdictionAmbiguous", id).error};
    selected = &rule;
  }
  if (!selected)
    return {false, nullptr,
            common_failure("MilitaryStrongholdJurisdictionOriginalRequired", id,
                           ErrorKind::unsupported_capability)
                .error};
  return {true, &selected->jurisdiction_id, {}};
}
Outcome<PoliticalTerritoryControl> PoliticalTakeoverTerritoryAccess::
    military_jurisdiction_control_before_custody_release(
        const NamedMilitaryCustodyRelease &release,
        const StableId &jurisdiction) const {
  const auto &x = context_;
  const NamedMilitaryCustodyRelease *actual = nullptr;
  for (const auto &row : x.military.named_custody.closures) {
    if (row.id != release.id)
      continue;
    if (actual)
      return {
          false,
          {},
          common_failure("MilitaryCustodyReleaseSourceAmbiguous", release.id)
              .error};
    actual = &row;
  }
  const NamedMilitaryCustodyTerm *term = nullptr;
  for (const auto &row : x.military.named_custody.terms) {
    if (row.id != release.term_id)
      continue;
    if (term)
      return {
          false,
          {},
          common_failure("MilitaryCustodyTermSourceAmbiguous", release.term_id)
              .error};
    term = &row;
  }
  const auto *head = x.common.state().facts.find(release.id);
  const auto *term_head = x.common.state().facts.find(release.term_id);
  const auto *prior =
      x.common.state().facts.find(release.prior_custody_source.id);
  if (!x.actual_clock || actual != &release || !term ||
      term->actor_id != release.actor_id || release.id.empty() ||
      release.recorded_tick != release.released_at.half_day_tick || !head ||
      head->id != release.id || head->domain != "military" ||
      head->kind != "military_named_custody_released" || !term_head ||
      term_head->id != term->id || term_head->domain != "military" ||
      term_head->kind != "military_named_custody_term" || !prior ||
      prior->id != release.prior_custody_source.id ||
      prior->domain != release.prior_custody_source.domain ||
      prior->kind != release.prior_custody_source.kind)
    return {
        false,
        {},
        common_failure("MilitaryCustodyReleaseActualSourceRequired", release.id)
            .error};
  const auto &order = x.common.state().facts.order();
  const auto cut = std::find(order.begin(), order.end(), release.id);
  const auto opened = std::find(order.begin(), order.end(), term->id);
  const auto held =
      std::find(order.begin(), order.end(), release.prior_custody_source.id);
  if (cut == order.end() || opened == order.end() || held == order.end() ||
      opened >= cut || held >= cut)
    return {
        false,
        {},
        common_failure("MilitaryCustodyReleaseControlCutMismatch", release.id)
            .error};
  auto now = x.actual_clock->read_organization_authority_now();
  if (!now.ok)
    return {false, {}, now.error};
  auto future = compare_world_moments(release.released_at, now.value);
  if (!future.ok)
    return {false, {}, future.error};
  if (future.value > 0)
    return {false,
            {},
            common_failure("MilitaryCustodyReleaseInFuture", release.id).error};
  return control_before_verified_original(jurisdiction, release.id,
                                          release.released_at);
}
Outcome<PoliticalTerritoryControl> PoliticalTakeoverTerritoryAccess::
    governance_jurisdiction_control_before_approval(
        const GovernanceProgramState &programs,
        const GovernanceProgramApproval &approval) const {
  const auto &x = context_;
  StableId id;
  if (const auto *patrol = std::get_if<GovernancePatrol>(&approval.target))
    id = patrol->jurisdiction_id;
  else if (const auto *pacification =
               std::get_if<GovernancePacification>(&approval.target))
    id = pacification->jurisdiction_id;
  else
    return {false,
            {},
            common_failure("GovernanceJurisdictionTargetRequired", approval.id)
                .error};
  const SourceRef source = governance_program_source(approval);
  const auto *head = x.common.state().facts.find(source.id);
  const auto &order = x.common.state().facts.order();
  const auto cut = std::find(order.begin(), order.end(), source.id);
  if (programs.approvals.find(approval.id) != &approval || !head ||
      head->domain != source.domain || head->kind != source.kind ||
      cut == order.end() || !x.actual_clock)
    return {false,
            {},
            common_failure("GovernanceJurisdictionActualApprovalRequired",
                           source.id)
                .error};
  auto now = x.actual_clock->read_organization_authority_now();
  if (!now.ok)
    return {false, {}, now.error};
  auto future = compare_world_moments(approval.issued_at, now.value);
  if (!future.ok)
    return {false, {}, future.error};
  if (future.value > 0)
    return {false,
            {},
            common_failure("GovernanceJurisdictionApprovalInFuture", source.id)
                .error};
  return control_before_verified_original(id, source.id, approval.issued_at);
}
Outcome<PoliticalTerritoryControl> PoliticalTakeoverTerritoryAccess::
    public_budget_jurisdiction_control_before_approval(
        const PoliticalBudgetState &state,
        const BudgetAuthorizationSource &approval, const StableId &id) const {
  const auto &x = context_;
  const auto &ref = approval.approval_source;
  if (state.schema_version != 2 ||
      find_budget_authorization(state, approval.budget_id,
                                approval.budget_revision) != &approval ||
      !x.actual_clock)
    return {false,
            {},
            common_failure("PublicBudgetActualApprovalRequired", ref.id).error};
  auto valid = validate_budget_authorization_shape(approval, 2);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *head = x.common.state().facts.find(ref.id);
  const char *kind = nullptr;
  switch (approval.decision) {
  case PublicBudgetDecisionKind::authorize:
    kind = "public_budget_authorized";
    break;
  case PublicBudgetDecisionKind::delegate:
    kind = "public_budget_delegated";
    break;
  case PublicBudgetDecisionKind::amend:
    kind = "public_budget_amended";
    break;
  case PublicBudgetDecisionKind::close:
    kind = "public_budget_closed";
    break;
  }
  if (!kind || !head || head->domain != "politics" || head->kind != kind ||
      ref.domain != head->domain || ref.kind != head->kind)
    return {false,
            {},
            common_failure("PublicBudgetActualApprovalRequired", ref.id).error};
  auto now = x.actual_clock->read_organization_authority_now();
  if (!now.ok)
    return {false, {}, now.error};
  auto future = compare_world_moments(approval.approved_at, now.value);
  if (!future.ok)
    return {false, {}, future.error};
  if (future.value > 0)
    return {false,
            {},
            common_failure("PublicBudgetApprovalInFuture", ref.id).error};
  return control_before_verified_original(id, ref.id, approval.approved_at);
}
// 不创建占位接触事实，也不接受调用者自报 M；完整原控制算法仍唯一。
Outcome<PoliticalTerritoryControl> PoliticalTakeoverTerritoryAccess::
    military_jurisdiction_control_before_field_contact_install(
        const FieldExactContactReadContext &field,
        const StableId &jurisdiction) const {
  const auto &x = context_;
  if (&field.common != &x.common || &field.military != &x.military ||
      &field.military_catalog != &x.military_catalog || !x.actual_clock ||
      !field.clock.bound_to(x.common, x.military, field.field))
    return {false,
            {},
            common_failure("PoliticalFieldControlContextMismatch", jurisdiction)
                .error};
  auto field_now = field.clock.read_current_moment();
  if (!field_now.ok)
    return {false, {}, field_now.error};
  auto political_now = x.actual_clock->read_organization_authority_now();
  if (!political_now.ok)
    return {false, {}, political_now.error};
  auto same_now = compare_world_moments(field_now.value, political_now.value);
  if (!same_now.ok)
    return {false, {}, same_now.error};
  if (same_now.value != 0)
    return {false,
            {},
            common_failure("PoliticalFieldControlClockMismatch", jurisdiction)
                .error};
  auto prefix = read_current_common_fact_prefix(x.common);
  if (!prefix.ok)
    return {false, {}, prefix.error};
  auto result = governance_jurisdiction_control_current(jurisdiction);
  if (!result.ok)
    return result;
  auto unchanged = read_actual_common_fact_prefix(x.common, prefix.value);
  if (!unchanged.ok)
    return {false, {}, unchanged.error};
  auto tail = read_current_common_fact_prefix(x.common);
  if (!tail.ok)
    return {false, {}, tail.error};
  if (tail.value.common_fact_count != prefix.value.common_fact_count ||
      tail.value.last_common_fact != prefix.value.last_common_fact)
    return {false,
            {},
            common_failure("PoliticalFieldControlPrefixChanged", jurisdiction)
                .error};
  return result;
}
// Only owner-verified named cuts above/below can reach this shared replay.
Outcome<PoliticalTerritoryControl> PoliticalTakeoverTerritoryAccess::
    military_jurisdiction_control_before_initial_field_contact(
        const FieldExactContactReadContext &field,
        const FieldBattleCatalog &field_catalog, const SourceRef &source,
        const StableId &jurisdiction) const {
  const auto &x = context_;
  if (&field.common != &x.common || &field.military != &x.military ||
      &field.military_catalog != &x.military_catalog || !x.actual_clock ||
      !field.clock.bound_to(x.common, x.military, field.field) ||
      !same_original_catalog_identity(
          field_catalog.identity,
          x.organization_catalog.authority_catalog_identity))
    return {false,
            {},
            common_failure("PoliticalFieldControlContextMismatch", source.id)
                .error};
  auto field_now = field.clock.read_current_moment();
  if (!field_now.ok)
    return {false, {}, field_now.error};
  auto political_now = x.actual_clock->read_organization_authority_now();
  if (!political_now.ok)
    return {false, {}, political_now.error};
  auto same_now = compare_world_moments(field_now.value, political_now.value);
  if (!same_now.ok)
    return {false, {}, same_now.error};
  if (same_now.value != 0)
    return {
        false,
        {},
        common_failure("PoliticalFieldControlClockMismatch", source.id).error};
  auto original = read_field_initial_contact_actual_moment(
      x.common, x.military, field.field, field_catalog, source, field);
  if (!original.ok)
    return {false, {}, original.error};
  if (!original.value.field_contact ||
      !original.value.actual_field_contact_head ||
      !original.value.original_movement_contact || !original.value.occurred_at)
    return {false,
            {},
            common_failure("PoliticalFieldControlOriginalMissing", source.id)
                .error};
  auto future =
      compare_world_moments(*original.value.occurred_at, political_now.value);
  if (!future.ok)
    return {false, {}, future.error};
  if (future.value > 0)
    return {false,
            {},
            common_failure("PoliticalFieldControlOriginalInFuture", source.id)
                .error};
  // Original takeover validation and current-body reverse coverage precede
  // the historical fold. The current body is checked, never used as the past.
  return control_before_verified_original(jurisdiction, source.id,
                                          *original.value.occurred_at);
}
Outcome<PoliticalTerritoryControl>
PoliticalTakeoverTerritoryAccess::control_before_verified_original(
    const StableId &id, const StableId &source_id,
    const WorldMoment &actual_at) const {
  const auto &x = context_;
  const auto &order = x.common.state().facts.order();
  const auto cut = std::find(order.begin(), order.end(), source_id);
  if (cut == order.end())
    return {
        false,
        {},
        common_failure("PoliticalOriginalControlCutMissing", source_id).error};
  auto complete = governance_jurisdiction_control_current(id);
  if (!complete.ok)
    return complete;
  const auto &seed = *x.politics_catalog.jurisdictions.find(id);
  PoliticalTerritoryControl result{x.common.state().identity,
                                   id,
                                   seed.controlling_organization_id,
                                   {},
                                   seed.revision};
  for (auto at = order.begin(); at != order.end(); ++at) {
    const auto *registration = x.state.registrations.find(*at);
    if (!registration)
      continue;
    const auto *rule = x.catalog.rules.find(registration->rule_id);
    if (!rule || rule->jurisdiction_id != id)
      continue;
    if (x.catalog.version != 2 || !registration->registered_at)
      return {
          false,
          {},
          common_failure("PoliticalOriginalExactTakeoverRequired", *at).error};
    auto before =
        compare_world_moments(*registration->registered_at, actual_at);
    if (!before.ok)
      return {false, {}, before.error};
    if (at == cut || (at < cut && before.value > 0) ||
        (at > cut && before.value < 0))
      return {false,
              {},
              common_failure("PoliticalOriginalControlCausality", *at).error};
    if (at > cut)
      continue;
    if (registration->former_organization_id !=
            result.controlling_organization_id ||
        result.revision == INT64_MAX)
      return {
          false,
          {},
          common_failure("PoliticalOriginalControlHistoryMismatch", *at).error};
    ++result.revision;
    result.controlling_organization_id =
        registration->successor_organization_id;
    result.occupation_fact_id = registration->occupation_fact_id;
  }
  return {true, std::move(result), {}};
}
Status PoliticalTakeoverTerritoryAccess::validate_head_handover_seat2(
    const PoliticalHeadHandoverContext &handover,
    const PoliticalSuccessionPolicy &policy, Integer tick,
    const StableId &boundary) const {
  const auto &x = context_;
  const auto *initial =
      x.politics_catalog.jurisdictions.find(policy.jurisdiction_id);
  if (&x.common != &handover.common ||
      &x.organizations != &handover.organizations ||
      &x.organization_catalog != &handover.organization_catalog ||
      handover.state.version != 2 || !x.actual_clock || !handover.exact ||
      x.organization_runtime != &handover.exact->runtime || !initial ||
      initial->revision != 0 || initial->office_id != policy.leader_office_id ||
      initial->seat_place_id != policy.seat_place_id ||
      initial->political_organization_id != policy.organization_id ||
      initial->controlling_organization_id != policy.organization_id)
    return common_failure("HeadHandoverOriginalSeatContextMismatch", policy.id);
  auto now = political_head_handover_now(handover);
  if (!now.ok)
    return {false, {}, now.error};
  auto territory_now = x.actual_clock->read_organization_authority_now();
  if (!territory_now.ok)
    return {false, {}, territory_now.error};
  auto same = compare_world_moments(now.value, territory_now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("HeadHandoverOriginalSeatClockMismatch", policy.id);
  auto actual = governance_jurisdiction_control_current(policy.jurisdiction_id);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (boundary.empty()) {
    if (tick != now.value.half_day_tick)
      return common_failure("HeadHandoverOriginalSeatClockMismatch", policy.id);
  } else {
    const auto *header = x.common.state().facts.find(boundary);
    if (!header)
      return common_failure("HeadHandoverOriginalSeatCutMissing", boundary);
    SourceRef ref{header->domain, header->kind, boundary};
    auto at = read_political_head_handover_moment2(handover, ref);
    if (!at.ok)
      return {false, {}, at.error};
    if (at.value.half_day_tick != tick)
      return common_failure("HeadHandoverOriginalSeatClockMismatch", boundary);
    actual = control_before_verified_original(policy.jurisdiction_id, ref.id,
                                              at.value);
    if (!actual.ok)
      return {false, {}, actual.error};
  }
  // Original first-head policy never revives after loss and reconquest.
  return actual.value.revision == 0 &&
                 actual.value.controlling_organization_id ==
                     policy.organization_id
             ? common_success()
             : common_failure("PoliticalHeadHandoverPolicyContinuityRequired",
                              policy.id);
}

} // namespace hanmo::resident
