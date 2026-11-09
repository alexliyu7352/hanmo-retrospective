#pragma once
#include "return_exact.hpp"
#include "military_return_source.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
#include "resident/domains/travel/spatial_location_capture.hpp"
namespace hanmo::resident {
class NativeReturnPlacementRead final:public MilitaryActionPlacementRead,public MilitaryReturnSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const MovementExactState& movement_;const ActionElapsedStore& elapsed_;ExactReturnReadContext context_;
public:
 NativeReturnPlacementRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const MovementExactState& movement,const ActionElapsedStore& elapsed,ExactReturnReadContext x):common_(c),military_(m),state_(s),movement_(movement),elapsed_(elapsed),context_(x){}
 bool bound_to(const CommonStore& c,const MovementExactState& m)const noexcept override{return &c==&common_&&&m==&movement_;}
 bool bound_to(const CommonStore& c,const MilitaryState& m)const noexcept override{return &c==&common_&&&m==&military_;}
 Outcome<std::optional<MilitaryCompletedReturnSource>> read_exact_army_return(const StableId&)const override;
 Outcome<WorldMoment> read_military_action_placement_source_moment(const MomentOwnerSource&)const override;
 Outcome<std::optional<MilitaryActionPlacement>> read_military_action_placement(const MomentOwnerSource&,const StableId&)const override;
 Outcome<CheckpointVector<MilitaryActionPlacement>> read_military_action_placement_history(const StableId&)const override;
};
Outcome<std::vector<BoundSpatialLocationWrite>> adopt_exact_return_spatial_bindings_in_candidate(
 const CommonStore&,MovementExactState&,const MilitaryState&,const MilitaryActionTimeState&,
 const ActionElapsedStore&,const ExactReturnReadContext&,const StableId& actual_action,
 Integer first_actual_location_sequence);
}
