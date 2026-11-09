#pragma once
#include "movement_exact_model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class CommonStore;
// Finite original completion-application heads. Kind classification alone is
// never proof of a row, Moment, or Actor placement.
inline bool is_military_action_placement_source_kind(const SourceRef& r)noexcept {
 return r.domain=="military"&&(r.kind=="military_action_completion_applied"||r.kind=="military_logistics_completion_applied");
}
// These finite borrowed inputs describe only the original formation/return
// placement producers. They do not add another Location or MovementAnchor owner.
using MilitaryActionPlacementLocationBuildView=std::variant<SettlementLocationBuildView,InArmyLocationBuildView>;
struct MilitaryActionPlacementAnchorBuildView {
 SettlementLocationBuildView location;SourceRefBuildView actual_source;
};
struct MilitaryActionPlacementBuildView {
 const StableId& actor_id;const WorldMoment& occurred_at;
 MomentOwnerSourceBuildView actual_application;
 MilitaryActionPlacementLocationBuildView original_location_result;
 MilitaryActionPlacementAnchorBuildView actual_anchor;
};


struct MilitaryActionPlacement {
 CheckpointMemoryCharge copied_fields;
 StableId actor_id;WorldMoment occurred_at;MomentOwnerSource actual_application;
 Location original_location_result;MovementAnchor actual_anchor;
 MilitaryActionPlacement()=default;
 explicit MilitaryActionPlacement(const MilitaryActionPlacementBuildView&);
 MilitaryActionPlacement(const MilitaryActionPlacement&)=default;
 MilitaryActionPlacement(MilitaryActionPlacement&& other):MilitaryActionPlacement(static_cast<const MilitaryActionPlacement&>(other)){}
 MilitaryActionPlacement& operator=(const MilitaryActionPlacement&)=delete;
 MilitaryActionPlacement& operator=(MilitaryActionPlacement&&)=delete;
};
// S2 original completed row owns these placements. This read never derives a
// historical result from today's Army, Actor Location, or current commander.
class MilitaryActionPlacementRead {
public:
 virtual ~MilitaryActionPlacementRead()=default;
 virtual bool bound_to(const CommonStore&,const MovementExactState&)const noexcept=0;
 virtual Outcome<WorldMoment> read_military_action_placement_source_moment(const MomentOwnerSource&)const=0;
 // null is only a verified source that did not place this particular Actor.
 virtual Outcome<std::optional<MilitaryActionPlacement>> read_military_action_placement(
  const MomentOwnerSource& actual_application,const StableId& actor_id)const=0;
 virtual Outcome<CheckpointVector<MilitaryActionPlacement>> read_military_action_placement_history(
  const StableId& actor_id)const=0;
};
}
