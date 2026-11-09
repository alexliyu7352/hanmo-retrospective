// 提案只产生原业务/原旅行下一步；预算与采用仍由原政治事业Director拥有。
#pragma once
#include "political_subordinate_appointment_rules.hpp"
#include "resident/domains/politics/politics_navigation.hpp"
namespace hanmo::resident {
struct PoliticalSubordinateAppointmentChoice {
 std::optional<AuthoredSubordinateAppointmentCommand> command;std::optional<TravelNextStep> travel;
 StableId intent_id,source_fact_id,target_place_id,reason;bool completed=false;Integer inspected=0;
};
// 调用者已让原Task/已受领承诺先行；该函数另保真实Action/会话，远方只用本人已有来源或作者原岗位路线。
Outcome<PoliticalSubordinateAppointmentChoice> propose_political_subordinate_appointment(const PoliticalSubordinateAppointmentContext&,const PoliticalNavigationRead&,const StableId& actor,const StableId& director_policy_id,Integer candidate_budget);
}
