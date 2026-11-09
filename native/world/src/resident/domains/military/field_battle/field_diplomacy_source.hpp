#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
// Actual cross-domain source projection; absence of a side is a verified
// third party, never a missing provider/default Neutral response.
struct FieldReinforcementDiplomacyRead {
  CheckpointMemoryCharge copied_fields;
  std::optional<Integer> side;
  std::vector<SourceRef> actual_relation_sources;
  // Same-faction support is original identity, not a fabricated self-Allied
  // pair.
  std::optional<SourceRef> same_faction_formation_source;
};
Outcome<FieldReinforcementDiplomacyRead>
read_current_field_reinforcement_diplomacy(
    const PoliticalRelationsReadContext &, const StableId &battle,
    const StableId &army, ExactArithmeticBudget = {});
Outcome<FieldReinforcementDiplomacyRead>
read_field_reinforcement_diplomacy_before_contact(
    const PoliticalRelationsReadContext &,
    const SourceRef &actual_boundary_contact, ExactArithmeticBudget = {});
} // namespace hanmo::resident
