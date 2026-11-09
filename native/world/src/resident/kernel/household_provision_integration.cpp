#include "native_family_initial_sources.hpp"
#include "household_provision_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
bool household_provision_management_enabled(const WorldBundle &world) {
  return world.catalog->household_provision_management &&
         world.catalog->household_provision_management->version == 1 &&
         world.catalog->family && world.catalog->family->household_control &&
         world.catalog->local_economy &&
         world.catalog->local_economy->household_provisioning;
}
Outcome<HouseholdProvisionConfigurationQuote>
prepare_household_provision_configuration(
    const WorldBundle &world, const StableId &actor,
    const ConfigureHouseholdProvisionCommand &command) {
  if (!household_provision_management_enabled(world))
    return {false,
            {},
            common_failure("HouseholdProvisionManagementUnavailable", {},
                           ErrorKind::unsupported_capability)
                .error};
  if (command.actor_id != actor)
    return {false, {}, common_failure("HouseholdProvisionActorMismatch").error};
  auto family_sources = NativeFamilyInitialSourcesLease::create(world);
  if (!family_sources.ok)
    return {false, {}, std::move(family_sources.error)};
  auto controls = world.household_control_read(
      family_sources.value ? &family_sources.value->initial_age() : nullptr);
  return preview_configure_household_provision(
      world.common, world.local_economy, *world.catalog->local_economy,
      *world.catalog->household_provision_management,
      world.household_provision_management, controls, command);
}
Outcome<HouseholdProvisionManagementEffects>
apply_household_provision_configuration(
    WorldBundle &world, const StableId &actor,
    const ConfigureHouseholdProvisionCommand &command) {
  auto preview =
      prepare_household_provision_configuration(world, actor, command);
  if (!preview.ok)
    return {false, {}, preview.error};
  auto family_sources = NativeFamilyInitialSourcesLease::create(world);
  if (!family_sources.ok)
    return {false, {}, std::move(family_sources.error)};
  auto controls = world.household_control_read(
      family_sources.value ? &family_sources.value->initial_age() : nullptr);
  return apply_configure_household_provision_in_candidate(
      world.common, world.local_economy, *world.catalog->local_economy,
      *world.catalog->household_provision_management,
      world.household_provision_management, controls, command);
}
Status validate_household_provision_world(const WorldBundle &world) {
  if (!household_provision_management_enabled(world)) {
    const auto &state = world.household_provision_management;
    for (const auto &history : world.common.state().history)
      if (history.kind == "configure_household_provision")
        return common_failure("UnexpectedHouseholdProvisionHistory");
    for (const auto &row : world.common.state().results.rows())
      if (row.second &&
          (row.second->kind == "configure_household_provision" ||
           (row.second->details &&
            std::holds_alternative<HouseholdProvisionManagementEffects>(
                row.second->details->data))))
        return common_failure("UnexpectedHouseholdProvisionReceipt");
    if (state.version != 1 ||
        world.household_provision_management_seed_complete ||
        world.catalog->household_provision_management ||
        !state.configurations.empty() || !state.resolutions.empty() ||
        !state.current_by_authorization.empty())
      return common_failure("HouseholdProvisionManagementProfileMismatch");
    return common_success();
  }
  if (!world.household_provision_management_seed_complete)
    return common_failure("HouseholdProvisionManagementSeedMissing");
  auto family_sources = NativeFamilyInitialSourcesLease::create(world);
  if (!family_sources.ok)
    return {false, {}, std::move(family_sources.error)};
  auto controls = world.household_control_read(
      family_sources.value ? &family_sources.value->initial_age() : nullptr);
  return validate_household_provision_management(
      world.common, world.local_economy, *world.catalog->local_economy,
      *world.catalog->household_provision_management,
      world.household_provision_management, controls);
}
HouseholdProvisionViewIndex
rebuild_household_provision_index(const WorldBundle &world) {
  HouseholdProvisionViewIndex empty;
  empty.identity = world.common.state().identity;
  empty.observer_actor_id = world.common.state().metadata.player_actor_id;
  if (!household_provision_management_enabled(world))
    return empty;
  auto family_sources = NativeFamilyInitialSourcesLease::create(world);
  if (!family_sources.ok) {
    empty.build_error = std::move(family_sources.error);
    return empty;
  }
  auto controls = world.household_control_read(
      family_sources.value ? &family_sources.value->initial_age() : nullptr);
  auto result = build_household_provision_view_index(
      world.common, world.local_economy, *world.catalog->local_economy,
      *world.catalog->household_provision_management,
      world.household_provision_management, controls);
  if (!result.ok) {
    empty.build_error = result.error;
    return empty;
  }
  return std::move(result.value);
}
Outcome<BoundaryValue>
query_household_provision_world(const WorldBundle &world,
                                const HouseholdProvisionViewIndex &index,
                                const BoundaryValue &args) {
  if (!household_provision_management_enabled(world))
    return {false,
            {},
            common_failure("HouseholdProvisionManagementUnavailable", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto query = decode_household_provision_management_query(
      args, world.common.state().metadata.player_actor_id);
  if (!query.ok)
    return {false, {}, query.error};
  auto family_sources = NativeFamilyInitialSourcesLease::create(world);
  if (!family_sources.ok)
    return {false, {}, std::move(family_sources.error)};
  auto controls = world.household_control_read(
      family_sources.value ? &family_sources.value->initial_age() : nullptr);
  auto page = query_household_provision_management(
      world.common, world.local_economy, *world.catalog->local_economy,
      *world.catalog->household_provision_management,
      world.household_provision_management, controls, index, query.value);
  if (!page.ok)
    return {false, {}, page.error};
  return {true, encode_household_provision_management_page(page.value), {}};
}
Status
validate_household_provision_configuration_receipts(const WorldBundle &world) {
  std::set<StableId> used;
  for (const auto &row : world.common.state().results.rows()) {
    if (!row.second)
      continue;
    const auto &result = *row.second;
    const auto *effects =
        result.details ? std::get_if<HouseholdProvisionManagementEffects>(
                             &result.details->data)
                       : nullptr;
    if (!effects) {
      if (result.kind == "configure_household_provision")
        return common_failure("HouseholdProvisionReceiptMismatch", row.first);
      continue;
    }
    if (!household_provision_management_enabled(world) ||
        result.kind != "configure_household_provision" ||
        effects->fact_ids.empty() || effects->fact_ids.size() > 2)
      return common_failure("HouseholdProvisionReceiptMismatch", row.first);
    const auto split = row.first.rfind(':');
    if (split == StableId::npos)
      return common_failure("HouseholdProvisionReceiptMismatch", row.first);
    const auto actor = row.first.substr(0, split);
    const HistoryRecord *history = nullptr;
    const HistoryRecord *before = nullptr;
    const HistoryRecord *previous = nullptr;
    for (const auto &h : world.common.state().history) {
      if (h.result_id == result.result_id) {
        if (history)
          return common_failure("HouseholdProvisionReceiptMismatch", row.first);
        history = &h;
        before = previous;
      }
      previous = &h;
    }
    if (!history || history->actor_id != actor ||
        history->kind != result.kind || history->tick != result.tick)
      return common_failure("HouseholdProvisionReceiptMismatch", row.first);
    const auto upper =
                   organization_instance_sequence(result.result_id, "result"),
               lower = before ? organization_instance_sequence(
                                    before->result_id, "result")
                              : std::optional<Integer>{0};
    if (!upper || !lower)
      return common_failure("HouseholdProvisionReceiptAllocationMismatch",
                            result.result_id);
    std::vector<StableId> expected;
    for (const auto &fact : world.common.state().facts.rows()) {
      if (fact.second.kind != "household_provision_configured" &&
          fact.second.kind != "household_provision_configuration_cancelled" &&
          fact.second.kind != "household_provision_configuration_superseded")
        continue;
      const auto sequence = organization_instance_sequence(
          fact.first, "household_provision_configuration");
      if (sequence && *sequence > *lower && *sequence < *upper)
        expected.push_back(fact.first);
    }
    // Only cached commands require their full exact effect list. Evicted
    // receipts do not erase old plans.
    if (expected != effects->fact_ids)
      return common_failure("HouseholdProvisionReceiptCoverageMismatch",
                            result.result_id);
    for (const auto &id : effects->fact_ids) {
      const auto sequence = organization_instance_sequence(
          id, "household_provision_configuration");
      if (!upper || !lower || !sequence || *sequence <= *lower ||
          *sequence >= *upper || !used.insert(id).second)
        return common_failure("HouseholdProvisionReceiptAllocationMismatch",
                              id);
      const auto &state = world.household_provision_management;
      if (const auto *config = state.configurations.find(id)) {
        if (config->actor_id != actor ||
            config->requested_tick != result.tick || result.revision < 1 ||
            config->requested_revision != result.revision - 1)
          return common_failure("HouseholdProvisionReceiptMismatch", id);
      } else {
        const auto *resolution = state.resolutions.find(id);
        if (!resolution || resolution->actor_id != actor ||
            resolution->tick != result.tick || result.revision < 1 ||
            resolution->source_revision != result.revision - 1)
          return common_failure("HouseholdProvisionReceiptMismatch", id);
      }
    }
  }
  return common_success();
}
} // namespace hanmo::resident
