#pragma once
#include "dynamic_session_model.hpp"
namespace hanmo::resident {
// One call to the original range service. No time advance and no implicit re-seeding.
Outcome<DynamicMilitaryRandomSlot> draw_dynamic_military_random_in_candidate(DynamicMilitarySessionRandom&,const SourceRef& actual_effect,DynamicMilitaryRandomStage,Integer minimum,Integer maximum);
Status validate_dynamic_military_random_slot(const DynamicMilitaryRandomSlot&);
Status validate_dynamic_military_random_chain(const DynamicMilitarySessionRandom& before,const std::vector<DynamicMilitaryRandomSlot>&,const DynamicMilitarySessionRandom& after);
Status validate_dynamic_military_random_state(const DynamicMilitarySessionRandom&);
// Complete identity/value equality for an already source-validated original
// slot. This is not proof of its occurrence and never draws/replays a stream.
bool same_dynamic_military_random_slot(const DynamicMilitaryRandomSlot&,const DynamicMilitaryRandomSlot&);

}
