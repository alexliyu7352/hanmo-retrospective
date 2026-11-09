#pragma once
#include "formation_exact.hpp"
#include "military_formation_source.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
#include "resident/domains/travel/spatial_location_capture.hpp"
namespace hanmo::resident {
class NativeFormationPlacementRead final:public MilitaryActionPlacementRead,public MilitaryFormationSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const MovementExactState& movement_;const ActionElapsedStore& elapsed_;ExactFormationReadContext context_;
public:
 NativeFormationPlacementRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const MovementExactState& movement,const ActionElapsedStore& elapsed,ExactFormationReadContext x):common_(c),military_(m),state_(s),movement_(movement),elapsed_(elapsed),context_(x){}
 bool bound_to(const CommonStore& c,const MovementExactState& m)const noexcept override{return &c==&common_&&&m==&movement_;}
 bool bound_to(const CommonState& c,const MilitaryState& m)const noexcept override{return &c==&common_.state()&&&m==&military_;}
 Outcome<std::optional<MilitaryFormationSourceView>> read_exact_army_formation(const StableId&)const override;
 Outcome<WorldMoment> read_military_action_placement_source_moment(const MomentOwnerSource&)const override;
 Outcome<std::optional<MilitaryActionPlacement>> read_military_action_placement(const MomentOwnerSource&,const StableId&)const override;
 Outcome<CheckpointVector<MilitaryActionPlacement>> read_military_action_placement_history(const StableId&)const override;
};
// Runs only immediately after the real completed row/header and actual Actor
// writes in this candidate. Returns each original tap for the outer capture.
Outcome<std::vector<BoundSpatialLocationWrite>> adopt_exact_formation_spatial_bindings_in_candidate(
 const CommonStore&,MovementExactState&,const MilitaryState&,const MilitaryActionTimeState&,
 const ActionElapsedStore&,const ExactFormationReadContext&,const StableId& actual_action,
 Integer first_actual_location_sequence);
}
