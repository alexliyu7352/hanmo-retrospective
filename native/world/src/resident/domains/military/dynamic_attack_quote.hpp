#pragma once
#include "battle_model.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
// Ephemeral exact-geometry read. Topology is solely the last actually reached
// vertex; range/facing/LOS come from the actual continuous local body positions.
struct DynamicMilitaryAttackGeometry {
 CanonicalFraction axial_distance;HexPoint attacker_topology,target_topology;
 Integer direction_from_target_to_attacker=0;bool line_of_sight=false;
};
struct DynamicMilitaryCommandedQualification {
 Integer attacker_leadership=0,attacker_skill_level=0,attacker_training=0;
 Integer defender_leadership=0,defender_skill_level=0,defender_training=0;
};
struct DynamicMilitaryAttackConditions {
 bool attacker_inspired=false,attacker_suppressed=false,target_exposed=false;
 std::optional<Integer> actual_attack_fatigue;
 std::optional<DynamicMilitaryCommandedQualification> commanded;
 bool actual_learned_guard=false;Integer actual_guard=0,actual_incoming_bp=10000;
 Integer actual_completed_flank_edges=0;std::optional<Integer> original_ranged_wall_bp;
};
// Pure quote only. The complete same-M producer first verifies actual entry,
// eligibility/geometry/condition sources, including an already-released effect.
// The original legacy formula is shared, and this call never executes a round.
Outcome<MilitaryAttackQuote> preview_dynamic_military_land_attack(
 const BattleUnit& original_attacker_snapshot,const BattleUnit& original_target_snapshot,
 const MilitaryBattleCatalog&,const StableId& battlefield_id,const StableId& actual_tactic_id,
 const DynamicMilitaryAttackGeometry&,const DynamicMilitaryAttackConditions&,
 ExactArithmeticBudget budget={});
}
