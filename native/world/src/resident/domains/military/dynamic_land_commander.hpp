// Original field_exposure_v1 at one complete exact-M land consequence barrier.
#pragma once
#include "dynamic_land_postures.hpp"
#include "dynamic_session_random.hpp"
namespace hanmo::resident {
// Derived original consequence; stored only by the existing casualty owner.
// Commanders outside member_ids are never inserted into a soldier roster.
struct DynamicLandCommanderConsequence {
 StableId unit_ref,army_ref,actor_ref;Integer before_loss=0,after_loss=0;
 bool actual_enlisted_member=false;SourceRef actual_effect_group;
};
struct DynamicLandCommanderMerge {
 std::vector<DynamicLandCommanderConsequence> consequences;
 std::vector<DynamicMilitaryRandomSlot> rng_slots;
 DynamicMilitarySessionRandom rng_after;
};
Outcome<DynamicLandCommanderMerge> merge_dynamic_land_commander_exposure(
 const MilitaryBattleCatalog&,const StableId& actual_battlefield,
 const std::vector<DynamicLandPostureUnit>& original_before,
 const DynamicLandMergedAllocation& complete_allocations,
 const SourceRef& actual_effect_group,const DynamicMilitarySessionRandom& after_strikes);
}
