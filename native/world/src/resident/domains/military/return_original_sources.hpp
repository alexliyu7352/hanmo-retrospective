#pragma once
#include "return_exact.hpp"
#include "military_return_source.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
namespace hanmo::resident {
struct MilitaryFieldHistoryRead;
// Only actual source/result/placement dependencies, never an unconstructed
// ExactReturnOriginalRead. All borrowed roots/providers must share one live
// World lease; this leaf does not perform complete Return admission.
struct ExactReturnSourceReadContext {
 const MilitaryCatalog& catalog;const MilitaryAccess& access;const TimeScaleRef& scale;
 const CurrentMomentRead& clock;const MilitaryExactState* exact;
 const MilitaryFormationSourceRead& formations;ExactArithmeticBudget budget{};
 const CatalogIdentity* identity=nullptr;const MilitaryFieldHistoryRead* field_history=nullptr;
 // The old exact-root constructor remains explicit; absence is only admitted
 // by the separate catalog-selected constructor, never by a null fallback.
 ExactReturnSourceReadContext(const MilitaryCatalog& c,const MilitaryAccess& a,const TimeScaleRef& s,const CurrentMomentRead& k,const MilitaryExactState& e,const MilitaryFormationSourceRead& f,ExactArithmeticBudget b={}):catalog(c),access(a),scale(s),clock(k),exact(&e),formations(f),budget(b){}
 ExactReturnSourceReadContext(const MilitaryCatalog& c,const MilitaryAccess& a,const TimeScaleRef& s,const CurrentMomentRead& k,const CatalogIdentity& i,const MilitaryExactState* e,const MilitaryFormationSourceRead& f,ExactArithmeticBudget b={},const MilitaryFieldHistoryRead* h=nullptr):catalog(c),access(a),scale(s),clock(k),exact(e),formations(f),budget(b),identity(&i),field_history(h){}
};
class NativeReturnSourceRead final:public MilitaryActionPlacementRead,public MilitaryReturnSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const MovementExactState& movement_;const ActionElapsedStore& elapsed_;ExactReturnSourceReadContext context_;
public:
 NativeReturnSourceRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const MovementExactState& movement,const ActionElapsedStore& elapsed,ExactReturnSourceReadContext x):common_(c),military_(m),state_(s),movement_(movement),elapsed_(elapsed),context_(x){}
 NativeReturnSourceRead(const NativeReturnSourceRead&)=delete;
 NativeReturnSourceRead& operator=(const NativeReturnSourceRead&)=delete;
 NativeReturnSourceRead(NativeReturnSourceRead&&)=delete;
 NativeReturnSourceRead& operator=(NativeReturnSourceRead&&)=delete;
 bool bound_to(const CommonStore& c,const MovementExactState& m)const noexcept override{return &c==&common_&&&m==&movement_;}
 bool bound_to(const CommonStore& c,const MilitaryState& m)const noexcept override{return &c==&common_&&&m==&military_;}
 Outcome<std::optional<MilitaryCompletedReturnSource>> read_exact_army_return(const StableId&)const override;
 Outcome<WorldMoment> read_military_action_placement_source_moment(const MomentOwnerSource&)const override;
 Outcome<std::optional<MilitaryActionPlacement>> read_military_action_placement(const MomentOwnerSource&,const StableId&)const override;
 Outcome<CheckpointVector<MilitaryActionPlacement>> read_military_action_placement_history(const StableId&)const override;
};
}
