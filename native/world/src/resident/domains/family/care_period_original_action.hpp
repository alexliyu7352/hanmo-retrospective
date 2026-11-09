// Source-local borrowed original family Action completion, without CarePeriod
// outputs, Condition effects or S5 feeding recursion.
#pragma once
#include "care_period.hpp"
#include "family_birth.hpp"
#include "child_care_relief.hpp"
namespace hanmo::resident {
struct CareOriginalActionPeriodSources {
 const CommonStore& common;const FamilyBirthState& births;
 const ChildCareReliefState* relief=nullptr;const CurrentMomentRead& clock;
};
struct CareOriginalActionPeriodView {
 const StableId* actor=nullptr;const StableId* child=nullptr;
 const StableId* action_id=nullptr;const StableId* accepted_id=nullptr;
 const StableId* place_id=nullptr;const FactRecord* original_start=nullptr;
 const FactRecord* original_completion=nullptr;
 Integer from_tick=0,to_tick=0,original_food_consumed=0;
 const ChildCareStart* child_care_start=nullptr;
 const ChildCareReceipt* child_care_receipt=nullptr;
 const ChildCareReliefStart* relief_start=nullptr;
 const ChildCareReliefReceipt* relief_receipt=nullptr;
};
// Only a genuine completed original one-half-day period is admitted. A longer
// legacy care Action is not split into invented per-half-day completions.
// Full original consent/resource validation and exact whole-span participation
// remain mandatory in the complete CarePeriodOriginalRead composition.
Outcome<CareOriginalActionPeriodView> read_care_original_action_period(
 const CareOriginalActionPeriodSources&,const OriginalCareActionParticipation&);
}
