// 逐政策事实分配序读取失控界线，恢复原组织控制不会自动复活此前失效政策。
#include "politics_takeover_fiscal.hpp"
#include "politics_takeover_detail.hpp"
#include "resident/domains/military/occupation_time.hpp"
#include "resident/domains/organization/office_authority.hpp"
namespace hanmo::resident {
Outcome<std::optional<FiscalPolicyOccupationBoundary>>
PoliticalTakeoverFiscalAccess::fiscal_policy_occupation(
    const FiscalPolicySnapshot &p) const {
  using namespace takeover_detail;
  const auto &x = context_;
  const auto owner_same = [](const Owner &a, const Owner &b) {
    return a.kind == b.kind && a.id == b.id;
  };
  const auto fail = [](const StableId &code, const StableId &id) {
    return Outcome<std::optional<FiscalPolicyOccupationBoundary>>{
        false, {}, common_failure(code, id).error};
  };
  auto valid = validate_political_takeover_state(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *initial =
      x.politics_catalog.jurisdictions.find(p.jurisdiction_id);
  const auto sequence =
      organization_instance_sequence(p.source_fact_id, "politics_fiscal");
  const auto *fact = x.common.state().facts.find(p.source_fact_id);
  if (!initial || initial->seat_place_id != p.site_id ||
      initial->political_organization_id != p.organization_id ||
      initial->public_wallet_id != p.public_wallet_id ||
      !has(initial->public_inventory_ids, p.public_inventory_id) || !sequence ||
      *sequence >= x.common.state().next_instance_id || !fact ||
      fact->kind != "political_fiscal_policy_changed" ||
      fact->domain != "politics")
    return fail("InvalidFiscalPolicyControlSource", p.source_fact_id);
  StableId controller = initial->controlling_organization_id;
  Owner wallet_owner{Owner::Kind::organization, p.organization_id},
      inventory_owner = wallet_owner;
  std::optional<FiscalPolicyOccupationBoundary> boundary;
  for (const auto &row : x.state.registrations.rows()) {
    const auto &r = row.second;
    const auto *rule = x.catalog.rules.find(r.rule_id);
    if (!rule || rule->jurisdiction_id != p.jurisdiction_id)
      continue;
    const auto number =
        organization_instance_sequence(r.occupation_fact_id, "military");
    const auto *occupation_fact = occupation(x.military, r.occupation_fact_id);
    if (!number || !occupation_fact || !occupation_fact->asset_manifest)
      return fail("InvalidFiscalPolicyControlSource", r.id);
    if (*number < *sequence) {
      if (r.tick > p.issued_tick)
        return fail("InvalidFiscalPolicyControlSource", p.source_fact_id);
      controller = r.successor_organization_id;
    } else if (!boundary && r.former_organization_id == p.organization_id) {
      if (r.tick < p.issued_tick)
        return fail("InvalidFiscalPolicyControlSource", p.source_fact_id);
      boundary = FiscalPolicyOccupationBoundary{x.common.state().identity,
                                                r.occupation_fact_id, r.tick};
      // /2 只借原军事发生时刻，不把既有 tick 投影提升为精确 M。
      if (x.catalog.version == 2) {
        if (!x.actual_clock)
          return fail("PoliticalTakeoverActualClockRequired",
                      r.occupation_fact_id);
        auto now = x.actual_clock->read_organization_authority_now();
        if (!now.ok)
          return {false, {}, now.error};
        auto actual = read_military_occupation_actual_moment(
            x.common, x.military, x.military_catalog,
            {"military", "military_occupation", r.occupation_fact_id}, 2,
            now.value);
        if (!actual.ok)
          return {false, {}, actual.error};
        boundary->actual_occupation_at = actual.value;
      }
    }
    for (const auto &asset : occupation_fact->asset_manifest->wallets)
      if (asset.first == p.public_wallet_id) {
        if (!owner_same(asset.second.from_owner, wallet_owner))
          return fail("FiscalPolicyAssetChainMismatch", asset.first);
        wallet_owner = asset.second.to_owner;
      }
    for (const auto &asset : occupation_fact->asset_manifest->inventories)
      if (asset.first == p.public_inventory_id) {
        if (!owner_same(asset.second.from_owner, inventory_owner))
          return fail("FiscalPolicyAssetChainMismatch", asset.first);
        inventory_owner = asset.second.to_owner;
      }
  }
  if (controller != p.organization_id)
    return fail("FiscalPolicyIssuedWithoutControl", p.source_fact_id);
  const auto *wallet = x.common.wallet(p.public_wallet_id);
  const auto *inventory = x.common.inventory(p.public_inventory_id);
  if (!wallet || !inventory || !owner_same(wallet->owner, wallet_owner) ||
      !owner_same(inventory->owner, inventory_owner))
    return fail("FiscalPolicyAssetChainMismatch", p.source_fact_id);
  return {true, std::move(boundary), {}};
}
} // namespace hanmo::resident
