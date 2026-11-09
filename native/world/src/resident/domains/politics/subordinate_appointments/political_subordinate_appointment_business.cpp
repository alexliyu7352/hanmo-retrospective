// 真实补任事实与原Office变更共同组成效果；不重复写职权或钱货。
#include "political_subordinate_appointment_business.hpp"
namespace hanmo::resident {
Outcome<PoliticsEffects> apply_political_subordinate_appointment_business_in_candidate(CommonStore& common,OrganizationState& org,PoliticalSubordinateAppointmentState& state,const PoliticalSubordinateAppointmentContext& x,const StableId& actor,const PoliticalSubordinateAppointmentCommand& command,const PoliticalSubordinateAppointmentQuote& quote){auto id=apply_political_subordinate_appointment_in_candidate(common,org,state,x,actor,command,quote);if(!id.ok)return {false,{},id.error};PoliticsEffects out;out.fact_ids={id.value};if(const auto* arrived=state.arrivals.find(id.value))out.fact_ids.push_back(arrived->office_change_fact_id);out.receipt_ids=out.fact_ids;return {true,std::move(out),{}};}
}
