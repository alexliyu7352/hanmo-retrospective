#include "resident/kernel/catalog_section_observation.hpp"
#include "political_relations.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include <set>
#include <string_view>
namespace hanmo::resident {
namespace {
bool pair_ok(const PoliticalFactionPair &p) {
  return !p.first.empty() && p.first < p.second;
}
bool relation_ok(PoliticalRelation r) { return static_cast<unsigned>(r) <= 2; }
} // namespace
Outcome<PoliticalFactionPair>
canonical_political_faction_pair(const StableId &a, const StableId &b) {
  if (a.empty() || b.empty() || a == b)
    return {false, {}, common_failure("InvalidPoliticalFactionPair").error};
  return {true,
          a < b ? PoliticalFactionPair{a, b} : PoliticalFactionPair{b, a},
          {}};
}
Status
validate_political_relations_catalog_shape(const PoliticalRelationsCatalog &c) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 106, "PoliticalRelationsCatalog", "validate_political_relations_catalog_shape", &c);

  using Pair = std::pair<std::string_view, std::string_view>;
  std::set<Pair, std::less<>, CheckpointAllocator<Pair>> seen;
  for (const auto &row : c.initial_relations) {
    if (!pair_ok(row.factions) || !relation_ok(row.relation))
      return common_failure("InvalidPoliticalInitialRelation");
    if (!seen.emplace(row.factions.first, row.factions.second).second)
      return common_failure("DuplicatePoliticalInitialRelation",
                            row.factions.first);
  }
  return common_success();
}
Status validate_political_relations_catalog(const PoliticalRelationsCatalog &c,
                                            const OrganizationCatalog &o) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 107, "PoliticalRelationsCatalog", "validate_political_relations_catalog", &c);

  auto valid = validate_political_relations_catalog_shape(c);
  if (!valid.ok)
    return valid;
  if (c.identity.content_digest.empty() ||
      !same_original_catalog_identity(c.identity, o.authority_catalog_identity))
    return common_failure("PoliticalRelationsCatalogIdentityMismatch");
  bool selected = false, catalog_selected = false;
  for (const auto &module : c.identity.module_versions) {
    if (module.first == "political_relations") {
      if (selected || module.second != "1")
        return common_failure("UnsupportedPoliticalRelationsSelection");
      selected = true;
    }
    if (module.first == "political_relations_catalog") {
      if (catalog_selected || module.second != "1")
        return common_failure("UnsupportedPoliticalRelationsCatalogSelection");
      catalog_selected = true;
    }
  }
  if (!selected || !catalog_selected)
    return common_failure("PoliticalRelationsSelectionRequired");
  for (const auto &row : c.initial_relations)
    if (!o.organizations.find(row.factions.first) ||
        !o.organizations.find(row.factions.second))
      return common_failure("PoliticalRelationFactionMissing",
                            row.factions.first);
  return common_success();
}
SourceRef political_battle_relation_change_source(
    const PoliticalBattleRelationChange &r) {
  return {"politics", "political_battle_relation_changed", r.id};
}
SourceRef political_alliance_breach_source(const PoliticalAllianceBreach &r) {
  return {"politics", "political_alliance_breached", r.id};
}
Status validate_political_relations_shape(const PoliticalRelationsState &s) {
  if (s.schema_version != 1 || !valid_source_ref(s.initialization) ||
      s.initialization.domain != "politics" ||
      s.initialization.kind != "political_relations_initialized")
    return common_failure("InvalidPoliticalRelationsState");
  auto at = encode_world_moment(s.initialized_at);
  if (!at.ok)
    return {false, {}, at.error};
  using Ids = std::set<std::string_view, std::less<>,
                       CheckpointAllocator<std::string_view>>;
  Ids battles, starts, breaches;
  for (const auto &row : s.battle_changes.rows()) {
    const auto &r = row.second;
    if (row.first != r.id || r.id.empty() || r.battle_id.empty() ||
        !pair_ok(r.factions) || !relation_ok(r.before) ||
        r.battle_start.source_schema != 1 ||
        !valid_source_ref(r.battle_start.source_ref) ||
        r.battle_start.source_ref.domain != "military" ||
        r.battle_start.source_ref.kind != "field_battle_started" ||
        !battles.insert(r.battle_id).second ||
        !starts.insert(r.battle_start.source_ref.id).second)
      return common_failure("InvalidPoliticalBattleRelationChange", row.first);
    at = encode_world_moment(r.occurred_at);
    if (!at.ok)
      return {false, {}, at.error};
    if (bool(r.alliance_breach) != (r.before == PoliticalRelation::allied))
      return common_failure("PoliticalAllianceBreachCoverageMismatch", r.id);
    if (r.alliance_breach) {
      const auto &ref = *r.alliance_breach;
      const auto *breach = s.alliance_breaches.find(ref.id);
      if (ref.domain != "politics" ||
          ref.kind != "political_alliance_breached" || !breach ||
          breach->id != ref.id ||
          (breach->relation_change.domain != "politics" ||
           breach->relation_change.kind !=
               "political_battle_relation_changed" ||
           breach->relation_change.id != r.id) ||
          !breaches.insert(ref.id).second)
        return common_failure("PoliticalAllianceBreachMismatch", r.id);
    }
  }
  for (const auto &row : s.alliance_breaches.rows())
    if (row.first != row.second.id || !breaches.count(row.first))
      return common_failure("OrphanPoliticalAllianceBreach", row.first);
  return common_success();
}
} // namespace hanmo::resident
