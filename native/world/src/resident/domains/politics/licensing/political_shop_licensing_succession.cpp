// 原法律1可选消费显式接册来源；无该读口仍严格保原控制与原Office，存档六表不增字段。
#include "political_shop_licensing_detail.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
// 每个fresh文本在构造前计账；这里只建立拥有值，来源门仍在唯一court读算法。
OwnedPoliticalShopRegistryEndpoint::OwnedPoliticalShopRegistryEndpoint(
    const StableId &court, const StableId &jurisdiction,
    const StableId &organization, const StableId &office, const StableId &site,
    const StableId &role, const StableId &dialogue, const StableId &node) {


  (memory).grow(0);
  value = {checkpoint_fresh_string_copy(court),
           checkpoint_fresh_string_copy(jurisdiction),
           checkpoint_fresh_string_copy(organization),
           checkpoint_fresh_string_copy(office),
           checkpoint_fresh_string_copy(site),
           checkpoint_fresh_string_copy(role),
           checkpoint_fresh_string_copy(dialogue),
           checkpoint_fresh_string_copy(node)};
}
} // namespace hanmo::resident
namespace hanmo::resident::shop_law_detail {
Outcome<OwnedPoliticalShopRegistryEndpoint>
court_endpoint_owned(const PoliticalShopLicensingContext &x,
                     const PoliticalShopCourt &court, Integer tick,
                     Integer sequence, const StableId &boundary) {
  const auto fail = [&](const StableId &code) {
    return Outcome<OwnedPoliticalShopRegistryEndpoint>{
        false, {}, common_failure(code, court.id).error};
  };
  const auto *j = x.politics_catalog.jurisdictions.find(court.jurisdiction_id);
  if (!j || tick < 0 || tick > x.common.tick() || sequence < 1 ||
      sequence > x.common.state().next_instance_id)
    return fail("PoliticalShopJurisdictionUnavailable");
  if (x.registry_successions) {
    auto source = boundary.empty()
                      ? x.registry_successions->shop_registry_current(court.id)
                      : x.registry_successions->shop_registry_before_legal_fact(
                            court.id, boundary, tick);
    if (!source.ok)
      return {false, {}, source.error};
    if (source.value) {
      const auto &s = *source.value;
      const auto &e = s.endpoint;
      const auto *root = registry(x.state, court.id);
      const auto *office = x.organization_catalog.offices.find(e.office_id);
      const auto *head = x.common.state().facts.find(s.acceptance_fact_id);
      auto number = organization_instance_sequence(
          s.acceptance_fact_id, political_shop_registry_accept_prefix);
      if (!same_identity(s.source_identity, x.common.state().identity) ||
          s.observed_tick != tick || s.accepted_tick < 0 ||
          s.accepted_tick > tick || !root ||
          s.initial_registry_fact_id != root->id || !number ||
          *number >= sequence || !head || head->id != s.acceptance_fact_id ||
          head->kind != political_shop_registry_accept_kind ||
          head->domain != political_shop_registry_succession_module ||
          (!boundary.empty() &&
           !before(x.common, s.acceptance_fact_id, boundary)) ||
          e.court_id != court.id ||
          e.jurisdiction_id != court.jurisdiction_id ||
          e.site_id != j->seat_place_id || !office ||
          office->organization_id != e.organization_id ||
          s.registration_fact_id.empty() || s.approval_fact_id.empty() ||
          s.occupation_fact_id.empty())
        return fail("PoliticalShopRegistrySourceMismatch");
      if (!s.registry_current)
        return fail("PoliticalShopRegistryContinuityRequired");
      return {true,
              OwnedPoliticalShopRegistryEndpoint(
                  e.court_id, e.jurisdiction_id, e.organization_id, e.office_id,
                  e.site_id, e.role_id, e.dialogue_id, e.request_node_id),
              {}};
    }
  }
  auto original = stable_court_before(x, court, tick, sequence, boundary);
  if (!original.ok)
    return {false, {}, original.error};
  return {true,
          OwnedPoliticalShopRegistryEndpoint(
              court.id, court.jurisdiction_id, j->political_organization_id,
              court.office_id, j->seat_place_id, court.role_id,
              court.dialogue_id, court.request_node_id),
          {}};
}
// 旧裸返回仅兼容，不作为正式内部拥有路径。
Outcome<PoliticalShopRegistryEndpoint>
court_endpoint(const PoliticalShopLicensingContext &x,
               const PoliticalShopCourt &court, Integer tick, Integer sequence,
               const StableId &boundary) {
  auto owned = court_endpoint_owned(x, court, tick, sequence, boundary);
  if (!owned.ok)
    return {false, {}, owned.error};
  return {true, owned.value.endpoint(), {}};
}
} // namespace hanmo::resident::shop_law_detail
