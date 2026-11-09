#pragma once
#include "movement_action_time.hpp"
namespace hanmo::resident {
Outcome<SourceRef> control_exact_personal_movement_in_candidate(CommonStore&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& actor_id,PersonalMovementControlOperation,const WorldMoment&,ExactArithmeticBudget budget={});
struct PersonalMovementControlRead {
    SourceRef actual_source;Integer source_schema=1;StableId actor_id,action_id;
    PersonalMovementControlOperation operation=PersonalMovementControlOperation::pause;
    WorldMoment occurred_at;SourceRef original_execution_source;
};
Outcome<PersonalMovementControlRead> read_personal_movement_control_source(const CommonRead&,const MovementExactState&,const SourceRef&,Integer schema,ExactArithmeticBudget budget={});
// Immutable adoption head of this real personal prefix (or retained successful
// write witness). Resume owns a new source while sharing the original geometry.
// Borrowed result is valid only while Common, Movement and the supplied prefix
// remain unchanged. This is the same original selector, not another authority.
Outcome<MomentOwnerSourceBuildView> read_personal_movement_immutable_head_view(
 const CommonRead&,const MovementExactState&,const MovementPrefixState&,
 const StableId& actor_id,ExactArithmeticBudget budget={});
Outcome<MomentOwnerSource> read_personal_movement_immutable_head(
 const CommonRead&,const MovementExactState&,const MovementPrefixState&,
 const StableId& actor_id,ExactArithmeticBudget budget={});
Status validate_personal_movement_controls(const CommonRead&,const MovementExactState&,ExactArithmeticBudget budget={});
}
