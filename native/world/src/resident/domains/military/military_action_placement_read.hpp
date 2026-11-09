#pragma once
#include "formation_spatial.hpp"
#include "return_spatial.hpp"
#include "formation_original_sources.hpp"
#include "return_original_sources.hpp"
namespace hanmo::resident {
// Two real original owners, borrowed from the same World lease. No copied row,
// fallback Actor location, or new placement history is retained here.
class NativeMilitaryActionPlacementRead final:public MilitaryActionPlacementRead {
 const MilitaryActionPlacementRead& formations_;const MilitaryActionPlacementRead& returns_;
 const MilitaryActionPlacementRead& owner(const MomentOwnerSource&)const;
public:
 NativeMilitaryActionPlacementRead(const NativeFormationPlacementRead& f,const NativeReturnPlacementRead& r):formations_(f),returns_(r){}
 NativeMilitaryActionPlacementRead(const NativeFormationSourceRead& f,const NativeReturnSourceRead& r):formations_(f),returns_(r){}
 bool bound_to(const CommonStore& c,const MovementExactState& m)const noexcept override{return formations_.bound_to(c,m)&&returns_.bound_to(c,m);}
 Outcome<WorldMoment> read_military_action_placement_source_moment(const MomentOwnerSource&)const override;
 Outcome<std::optional<MilitaryActionPlacement>> read_military_action_placement(const MomentOwnerSource&,const StableId&)const override;
 Outcome<CheckpointVector<MilitaryActionPlacement>> read_military_action_placement_history(const StableId&)const override;
};
}
