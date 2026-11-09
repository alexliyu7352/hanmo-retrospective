// 新意图复用原政治事业模板6与预算；短栈仅借同一真实World。
#pragma once
#include "political_subordinate_appointment_world.hpp"
#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_director.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_political_subordinate_appointment_world(const WorldBundle&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Outcome<PoliticalSubordinateAppointmentQuote> prepare_authored_subordinate_appointment_world(const WorldBundle&,const StableId&,const AuthoredSubordinateAppointmentCommand&);
Outcome<PoliticsEffects> commit_authored_subordinate_appointment_world(WorldBundle&,const StableId&,const AuthoredSubordinateAppointmentCommand&,const PoliticalSubordinateAppointmentQuote&);
Status validate_political_subordinate_appointment_director_world(const WorldBundle&);
}
