// P3 /1: the minimal ordinary relation owner approved in 3ecce425, not P2 treaties.
#pragma once
#include "resident/kernel/world_timeline.hpp"
#include "resident/domains/organization/organization_lifecycle_contract.hpp"
namespace hanmo::resident {
enum class PoliticalRelation { hostile, neutral, allied };
struct PoliticalFactionPair {
  StableId first, second; // canonical first < second; distinct existing factions
};
struct PoliticalInitialRelation {
  PoliticalFactionPair factions;
  PoliticalRelation relation = PoliticalRelation::neutral;
};
struct PoliticalRelationsCatalog {
  CatalogIdentity identity;
  std::vector<PoliticalInitialRelation> initial_relations;
};
struct PoliticalBattleRelationChange {
  StableId id, battle_id;
  PoliticalFactionPair factions;
  MomentOwnerSource battle_start;
  WorldMoment occurred_at;
  PoliticalRelation before = PoliticalRelation::neutral;
  // No after setter: every actual battle start makes this pair Hostile.
  std::optional<SourceRef> alliance_breach;
};
struct PoliticalAllianceBreach {
  StableId id;
  SourceRef relation_change;
  // Original pair/battle/clock are read from that one change, not copied.
};
struct PoliticalRelationsState {
  Integer schema_version = 1;
  SourceRef initialization;
  WorldMoment initialized_at;
  OrderedTable<PoliticalBattleRelationChange> battle_changes;
  OrderedTable<PoliticalAllianceBreach> alliance_breaches;
};
Outcome<PoliticalFactionPair> canonical_political_faction_pair(const StableId&, const StableId&);
Status validate_political_relations_catalog_shape(const PoliticalRelationsCatalog&);
Status validate_political_relations_catalog(const PoliticalRelationsCatalog&, const OrganizationCatalog&);
Status validate_political_relations_shape(const PoliticalRelationsState&);
SourceRef political_battle_relation_change_source(const PoliticalBattleRelationChange&);
SourceRef political_alliance_breach_source(const PoliticalAllianceBreach&);
// Paired physical /1; decoded catalog identity is injected from the locked job.
Status visit_political_relations(CodecVisitor&, PoliticalRelationsState&);
Status emit_political_relations_projection(ProjectionWriter&, std::string_view, const PoliticalRelationsState&);
Outcome<BoundaryValue> encode_political_relations(const PoliticalRelationsState&);
Outcome<PoliticalRelationsState> decode_political_relations(const BoundaryValue&);
Status visit_political_relations_catalog(CodecVisitor&, PoliticalRelationsCatalog&);
Status emit_political_relations_catalog_projection(ProjectionWriter&, std::string_view, const PoliticalRelationsCatalog&);
Outcome<BoundaryValue> encode_political_relations_catalog(const PoliticalRelationsCatalog&);
Outcome<PoliticalRelationsCatalog> decode_political_relations_catalog(const BoundaryValue&, const CatalogIdentity&, const OrganizationCatalog&);
}
