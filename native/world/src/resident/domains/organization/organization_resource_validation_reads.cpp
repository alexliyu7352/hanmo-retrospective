#include "organization_resource_validation_reads.hpp"
#include "resident/domains/military/occupation_rules.hpp"
#include "resident/domains/politics/governance_continuation/political_governance_continuation_declarations.hpp"
#include "resident/domains/politics/supply/politics_supply_authority.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal.hpp"
#include "resident/domains/politics/takeover/politics_takeover_authority.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
NativeOrganizationResourceValidationReads
make_native_organization_resource_validation_reads(
    const WorldBundle &world, const MilitaryAccess &access) {
  // 原 WorldBundle::validate_impl 两正文只作 World 实参限定，判断顺序不变。
  // 原调用已先完整核军事占领；同 job scope 必须保同一真实 access，而非临时值。
  OrganizationResourceHistoryRead history =
      [&world, &access](const ResourceKey &resource,
                        const StableId &original_organization) {
        const bool wallet = resource.kind == ResourceKey::Kind::wallet;
        if (!wallet && resource.kind != ResourceKey::Kind::inventory)
          return false;
        const auto *authored =
            wallet
                ? world.catalog->common->required_wallets.find(resource.id)
                : world.catalog->common->required_inventories.find(resource.id);
        if (!authored || authored->owner.kind != Owner::Kind::organization ||
            authored->owner.id != original_organization)
          return false;
        auto ownership = military_public_owner(
            world.common, world.military, *world.catalog->military, access,
            wallet ? "wallet_id" : "inventory_id", resource.id);
        if (!ownership.ok || ownership.value.occupation_id.empty())
          return false;
        const Owner *current = nullptr;
        if (wallet) {
          const auto *value = world.common.wallet(resource.id);
          if (value)
            current = &value->owner;
        } else {
          const auto *value = world.common.inventory(resource.id);
          if (value)
            current = &value->owner;
        }
        return current && owner_equal(*current, ownership.value.owner);
      };
  OrganizationProspectiveGrantRead prospective =
      [&world](const Grant &grant, const ResourceKey &resource) {
        return world.political_takeover_enabled() &&
               (political_takeover_grant_declares_resource(
                    *world.catalog->common, *world.catalog->organization,
                    *world.catalog->politics, *world.catalog->military,
                    *world.catalog->political_takeover, grant.id, resource) ||
                (world.occupation_fiscal_enabled() &&
                 occupation_fiscal_grant_declares_resource(
                     *world.catalog->common, *world.catalog->organization,
                     *world.catalog->politics, *world.catalog->military,
                     *world.catalog->political_takeover,
                     *world.catalog->occupation_fiscal, grant.id, resource)) ||
                (world.catalog->political_governance_continuations &&
                 governance_continuation_grant_declares_resource(
                     *world.catalog->common, *world.catalog->organization,
                     *world.catalog->politics, *world.catalog->military,
                     *world.catalog->political_takeover, true, grant.id,
                     resource)) ||
                (world.economic_supply_policies_enabled() &&
                 political_supply_grant_declares_resource(
                     *world.catalog->common, *world.catalog->organization,
                     *world.catalog->politics, *world.catalog->military,
                     *world.catalog->political_takeover, true, grant.id,
                     resource)));
      };
  return {std::move(history), std::move(prospective)};
}
} // namespace hanmo::resident
