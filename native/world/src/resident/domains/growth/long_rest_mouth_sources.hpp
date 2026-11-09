// The original LongRest per-period candidate selection. No per-child work timer.
#pragma once
#include "long_rest_period.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;struct RestFact;struct RestEstateView;
Outcome<std::vector<LongRestMouthInputs>> capture_long_rest_mouth_inputs(const CharacterConditionSources&,const RestFact&,const LongRestActualPeriod&,const RestEstateView&);
Status validate_long_rest_mouth_inputs(const CharacterConditionSources&,const RestFact&,const LongRestActualPeriod&,const RestEstateView&);
}
