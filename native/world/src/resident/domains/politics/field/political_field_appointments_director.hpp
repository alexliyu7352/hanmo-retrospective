// 野外自主签受只提出同军原业务命令；公平复评、采用及回滚仍归Director。
#pragma once
#include "political_field_appointments.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_field_appointment(const PoliticalFieldAppointmentContext&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Status validate_political_field_director(const DirectorCatalog&,const DirectorState&,const PoliticalFieldAppointmentCatalog*,const PoliticalFieldAppointmentState*);
}
