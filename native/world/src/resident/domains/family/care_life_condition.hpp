// Actual Life4/Condition4 -> original Care target/coverage, same candidate.
#pragma once
#include "care_coverage.hpp"
#include "care_participant_capacity.hpp"
namespace hanmo::resident {
struct CareLifeConditionApplication {Integer actual_goal_transitions=0,already_applied_transitions=0;};
bool care_life_condition_consumer_selected(const ActorFoundationCatalog&)noexcept;
// Each original Life writer calls this after that token's capacity application,
// before applying the next actor. A whole-capture sweep is not this contract.
class CareLifeConditionWrite {
public:virtual ~CareLifeConditionWrite()=default;
 virtual bool bound_to(const CommonStore&,const AuthoredActorFoundationState&,
  const FamilyDeathState&)const noexcept=0;
 virtual Status consume_actual_life_condition(const BoundActorConditionWrite&)=0;
};
struct CareLifeConditionWriteBinding {
 CommonStore& common;CareGoalWriteTables goals;CareCoverageWriteTables coverage;
 const CareGoalSourceContext& goal_sources;const CharacterConditionSources& body;
 AffairsState& affairs;const StateNeedPolicyRead& policies;
 const StateGoalRead& goal_read;StateTaskConsumer& tasks;
};
// The owning world constructs this with fresh source context at the actual M.
// Every borrowed owner is checked again by the original single-token writer.
class NativeCareLifeConditionWrite final:public CareLifeConditionWrite {
 CareLifeConditionWriteBinding binding_;
public:explicit NativeCareLifeConditionWrite(CareLifeConditionWriteBinding b):binding_(b){}
 bool bound_to(const CommonStore&,const AuthoredActorFoundationState&,
  const FamilyDeathState&)const noexcept override;
 Status consume_actual_life_condition(const BoundActorConditionWrite&)override;
};
// Called after this real token's capacity outputs were recorded, before the
// next external event. All affected original Care transitions are written
// before reconciling their actual StateNeed episodes. No second capacity-loss
// outcome is emitted for the same HolderDeath.
Outcome<CareLifeConditionApplication> apply_care_life_condition_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,
 const CareGoalSourceContext&,const CharacterConditionSources&,
 const BoundActorConditionWrite&,AffairsState&,const StateNeedPolicyRead&,
 const StateGoalRead&,StateTaskConsumer&);
// Required stable install/restore/publication gate, distinct from the private
// actual-source application above. It checks each real Death4 against the
// original goal/binding at that Common cut and its unique real consequence.
Status validate_care_life_condition_coverage(const CareCoverageSources&,
 const CharacterConditionSources&);
}
