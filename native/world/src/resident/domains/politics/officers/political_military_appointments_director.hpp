// 具名军官政策只提出原正式命令；公平轮次、复评预算与采用回滚仍归Director。
#pragma once
#include "political_military_appointments.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_military_appointment(const PoliticalMilitaryAppointmentContext&,const TravelCatalog&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Status validate_political_military_director(const DirectorCatalog&,const DirectorState&,const PoliticalMilitaryAppointmentCatalog*,const PoliticalMilitaryAppointmentState*);
}
