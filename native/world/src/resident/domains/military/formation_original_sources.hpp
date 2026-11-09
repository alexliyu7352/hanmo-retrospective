#pragma once
#include "formation_exact.hpp"
#include "military_formation_source.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
namespace hanmo::resident {
// The genuine completed formation/source and placement leaf has no dependency
// on ExactFormationOriginalRead. Complete acceptance/service/result validation
// remains the responsibility of that distinct owner and the stable World gate.
struct ExactFormationSourceReadContext {
 const MilitaryAccess& access;const TimeScaleRef& scale;const CurrentMomentRead& clock;
 ExactArithmeticBudget budget{};
};
class NativeFormationSourceRead final:public MilitaryActionPlacementRead,public MilitaryFormationSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const MovementExactState& movement_;const ActionElapsedStore& elapsed_;ExactFormationSourceReadContext context_;
public:
 NativeFormationSourceRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const MovementExactState& movement,const ActionElapsedStore& elapsed,ExactFormationSourceReadContext x):common_(c),military_(m),state_(s),movement_(movement),elapsed_(elapsed),context_(x){}
 NativeFormationSourceRead(const NativeFormationSourceRead&)=delete;
 NativeFormationSourceRead& operator=(const NativeFormationSourceRead&)=delete;
 NativeFormationSourceRead(NativeFormationSourceRead&&)=delete;
 NativeFormationSourceRead& operator=(NativeFormationSourceRead&&)=delete;
 bool bound_to(const CommonStore& c,const MovementExactState& m)const noexcept override{return &c==&common_&&&m==&movement_;}
 bool bound_to(const CommonState& c,const MilitaryState& m)const noexcept override{return &c==&common_.state()&&&m==&military_;}
 Outcome<std::optional<MilitaryFormationSourceView>> read_exact_army_formation(const StableId&)const override;
 Outcome<WorldMoment> read_military_action_placement_source_moment(const MomentOwnerSource&)const override;
 Outcome<std::optional<MilitaryActionPlacement>> read_military_action_placement(const MomentOwnerSource&,const StableId&)const override;
 Outcome<CheckpointVector<MilitaryActionPlacement>> read_military_action_placement_history(const StableId&)const override;
};
}
