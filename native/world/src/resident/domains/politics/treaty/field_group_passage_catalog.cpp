#include "resident/kernel/catalog_section_observation.hpp"
#include "field_group_passage_catalog.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
constexpr const char *approval = "總設計師 2026-10-29 Slack 批准";
bool same_boundary(const PassageBoundaryRef &a, const PassageBoundaryRef &b) {
  return a.kind == b.kind && a.id == b.id && a.map_id == b.map_id &&
         a.from_site_id == b.from_site_id && a.to_site_id == b.to_site_id;
}
bool same_catalog(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions &&
         a.artifact_sha256 == b.artifact_sha256;
}
// 原 boundary::fields 只检查未知字段，本入口再拒重复字段以免首值掩后值。
void unique_fields(const BoundaryValue &v,
                   std::initializer_list<const char *> names) {
  boundary::fields(v, names, {}, "field_group_passage");
  const auto &rows = boundary::object(v, "field_group_passage");
  for (std::size_t i = 0; i < rows.size(); ++i)
    for (std::size_t j = 0; j < i; ++j)
      if (rows[i].first == rows[j].first)
        boundary::fail("DuplicateFieldGroupPassageField", rows[i].first);
}
StableId text(const BoundaryValue &v, const char *key) {
  return boundary::get_string(v, key, "field_group_passage");
}
} // namespace
Status validate_field_group_passage_catalog(const FieldGroupPassageCatalog &c,
                                            const OrganizationCatalog &orgs,
                                            const TravelCatalog &travel) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 113, "FieldGroupPassageCatalog", "validate_field_group_passage_catalog", &c);

  if (c.schema_version != 1 || c.provenance != approval ||
      c.policies.size() != 4 || c.identity.content_digest.empty() ||
      !orgs.enabled ||
      !same_catalog(c.identity, orgs.authority_catalog_identity))
    return common_failure("FieldGroupPassageCatalogIdentityMismatch");
  std::set<StableId> ids;
  std::set<std::pair<StableId, StableId>> pairs;
  for (const auto &p : c.policies) {
    if (p.id.empty() || p.version != 1 || !ids.insert(p.id).second ||
        p.boundary.kind != PassageBoundaryKind::road_edge ||
        p.boundary.map_id != "map.qingchuan" ||
        (p.boundary.id != "road.west_east" &&
         p.boundary.id != "road.east_west") ||
        (p.organization_id != "org.qingchuan" &&
         p.organization_id != "org.blackwater") ||
        !pairs.emplace(p.boundary.id, p.organization_id).second)
      return common_failure("FieldGroupPassageFixtureScopeMismatch", p.id);
    const auto *org = orgs.organizations.find(p.organization_id);
    const auto *road = travel.roads.find(p.boundary.id);
    if (!org || org->id != p.organization_id || !road ||
        road->id != p.boundary.id || road->world_map_id != p.boundary.map_id ||
        !travel.maps.find(p.boundary.map_id) ||
        !travel.settlements.find(p.boundary.from_site_id) ||
        !travel.settlements.find(p.boundary.to_site_id) ||
        road->origin != p.boundary.from_site_id ||
        road->destination != p.boundary.to_site_id ||
        p.boundary.from_site_id == p.boundary.to_site_id)
      return common_failure("FieldGroupPassageOriginalBoundaryMismatch", p.id);
  }
  // 四个不重复元素来自唯一2×2集合，故完整覆盖而非缺项默认允许。
  return common_success();
}
Outcome<FieldGroupPassageCatalog> decode_field_group_passage_catalog(
    const BoundaryValue &input, const CatalogIdentity &identity,
    const OrganizationCatalog &orgs, const TravelCatalog &travel) {
  try {
    unique_fields(input, {"schema_version", "provenance", "policies"});
    FieldGroupPassageCatalog out;
    out.identity = identity;
    out.schema_version =
        boundary::get_int(input, "schema_version", "field_group_passage");
    out.provenance = text(input, "provenance");
    const auto &rows = boundary::array(
        boundary::field(input, "policies", "field_group_passage"),
        "field_group_passage/policies");
    if (rows.size() != 4)
      return {false,
              {},
              common_failure("FieldGroupPassageFourPoliciesRequired").error};
    for (const auto &r : rows) {
      unique_fields(r, {"id", "version", "boundary", "organization_id", "mode",
                        "admission", "tariff"});
      if (text(r, "mode") != "army" || text(r, "admission") != "allowed" ||
          text(r, "tariff") != "none")
        return {false,
                {},
                common_failure("FieldGroupPassageFixtureRuleMismatch").error};
      FieldGroupPassagePolicy p;
      p.id = text(r, "id");
      p.version =
          boundary::get_int(r, "version", "field_group_passage/policies");
      p.organization_id = text(r, "organization_id");
      const auto &b =
          boundary::field(r, "boundary", "field_group_passage/policies");
      unique_fields(b, {"kind", "id", "map_id", "from_site_id", "to_site_id"});
      if (text(b, "kind") != "road_edge")
        return {false,
                {},
                common_failure("FieldGroupPassageRoadBoundaryRequired").error};
      p.boundary.id = text(b, "id");
      p.boundary.map_id = text(b, "map_id");
      p.boundary.from_site_id = text(b, "from_site_id");
      p.boundary.to_site_id = text(b, "to_site_id");
      out.policies.push_back(std::move(p));
    }
    auto valid = validate_field_group_passage_catalog(out, orgs, travel);
    if (!valid.ok)
      return {false, {}, valid.error};
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<const FieldGroupPassagePolicy *>
read_field_group_passage_policy(const FieldGroupPassageCatalog &catalog,
                                const PassageBoundaryRef &boundary,
                                const StableId &organization) {
  const FieldGroupPassagePolicy *found = nullptr;
  for (const auto &p : catalog.policies)
    if (p.organization_id == organization &&
        same_boundary(p.boundary, boundary)) {
      if (found)
        return {false,
                {},
                common_failure("DuplicateFieldGroupPassagePolicy", p.id).error};
      found = &p;
    }
  if (!found)
    return {
        false,
        {},
        common_failure("FieldGroupPassageNotAuthorized", boundary.id).error};
  return {true, found, {}};
}
} // namespace hanmo::resident
