// Complete person sources for the original Family relationship/consent rules.
// No second life, age, relation or control state is owned here.
#pragma once
#include "family_life_history4.hpp"
#include "life_age.hpp"
#include "actor_presence_read.hpp"
namespace hanmo::resident {
struct ControlContinuationState;
class FamilyContinuationPeopleRead4 final {
 const LifeAgeSources& ages_;const FamilyLifeHistoryRead4& life_;const CurrentMomentRead& clock_;const ControlContinuationState* controls_=nullptr;
 Outcome<ActorPresenceRead> presence(const StableId&,Integer,const SourceRef*,bool,const CommonActualFactPrefix* =nullptr)const;
public:
 FamilyContinuationPeopleRead4(const LifeAgeSources& a,const FamilyLifeHistoryRead4& l,const CurrentMomentRead& t,const ControlContinuationState* controls=nullptr):ages_(a),life_(l),clock_(t),controls_(controls){}
 bool bound_to(const CommonStore&)const noexcept;
 bool bound_to(const CommonStore&,const FamilyDeathState&)const noexcept;
 const ControlContinuationState* control_continuation()const noexcept{return controls_;}
 const FamilyLifeHistoryRead4& life()const noexcept{return life_;}
 Outcome<WorldMoment> read_current_moment()const;
 Outcome<LifeAgeBasisSnapshot> read_current_age(const StableId&)const;
 Outcome<LifeAgeBasisSnapshot> read_age_before_original_fact(const StableId&,Integer,const SourceRef&)const;
 Outcome<LifeAgeBasisSnapshot> read_age_at_original_prefix(const StableId&,Integer,const CommonActualFactPrefix&)const;
 Outcome<ActorPresenceRead> read_presence_at_original_prefix(const StableId&,Integer,const CommonActualFactPrefix&)const;
 Outcome<ActorPresenceRead> read_current_presence(const StableId&)const;
 Outcome<ActorPresenceRead> read_presence_before_original_fact(const StableId&,Integer,const SourceRef&)const;
};
Outcome<FamilyDecisionAgeSources4> capture_family_decision_ages4(const FamilyContinuationPeopleRead4&,const StableId& predecessor,const StableId& successor);
Status validate_family_decision_ages4(const FamilyContinuationPeopleRead4&,const CommonStore&,const FamilyDecisionAgeSources4&,const StableId& predecessor,const StableId& successor,Integer original_tick,const StableId& actual_before_fact={});
}
