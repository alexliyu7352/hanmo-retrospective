#pragma once
#include "dynamic_session.hpp"
namespace hanmo::resident {
struct DynamicSessionExpiryDue {
 StableId session_ref;Integer generation=0;StableId unit_ref;
 WorldMoment at;SourceRef actual_activation_source;
 std::variant<DynamicStatusExpired,DynamicPostureClosed> original_transition;
};
// Original finite durations are absolute hard gates. Prediction names the
// actual retained activation, never a synthetic 'due' fact.
Outcome<std::vector<DynamicSessionExpiryDue>> collect_dynamic_session_expiries_at(
 const CommonStore&,const MilitaryExactState&,const DynamicMilitarySessionOriginalRead&,
 const WorldMoment& actual_current,const WorldMoment& at,ExactArithmeticBudget budget={});
Outcome<std::optional<WorldMoment>> predict_dynamic_session_expiry(
 const CommonStore&,const MilitaryExactState&,const DynamicMilitarySessionOriginalRead&,
 const WorldMoment& actual_current,ExactArithmeticBudget budget={});
// Uses the existing actual transition writer and the trusted exact phase M.
// All expiries at that M close before the effect group's before snapshot.
Outcome<std::vector<MomentOwnerSource>> apply_dynamic_session_expiries_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,
 const DynamicMilitarySessionOriginalRead&,const CurrentMomentRead&,ExactArithmeticBudget budget={});
}
