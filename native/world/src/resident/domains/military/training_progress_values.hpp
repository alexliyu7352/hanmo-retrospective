// Original C06 integer result values, common to legacy/exact writers and cold
// readers. S2 5.2 keeps this quantized business result separate from S0 elapsed.
#pragma once
#include "training_model.hpp"
namespace hanmo::resident::military_training_progress {
// Callers first validate the original terms and completed count [0,4]; lesson
// is [1,4]. These bounded arithmetic helpers neither authorize a lesson nor
// verify participation, supply, payment or an actual completion source.
inline Integer cumulative_fee(Integer completed) noexcept {return 150*completed/4;}
inline Integer lesson_fee(Integer lesson) noexcept {return cumulative_fee(lesson)-cumulative_fee(lesson-1);}
inline Integer unprovided_fee(Integer completed) noexcept {return 150-cumulative_fee(completed);}
inline Integer unit_fatigue(const MilitaryTrainingTerms& terms,Integer completed) noexcept {
 const Integer value=terms.fatigue_before+terms.fatigue_per_tick*completed;
 return value>100?100:value;
}
// The floor is applied to the original total gain before the frozen Drill
// multiplier, exactly as in the original rule. Validated gain <=7, bp<=12000.
inline Integer unit_training(const MilitaryTrainingTerms& terms,Integer completed,Integer frozen_bp) noexcept {
 const Integer normal=terms.gain_total*completed/4;
 const Integer value=terms.training_before+normal*frozen_bp/10000;
 return value>100?100:value;
}
}
