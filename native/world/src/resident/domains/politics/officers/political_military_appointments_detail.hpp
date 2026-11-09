// 临时审计索引不进入冷段；每次读取沿同Common顺序重建签发、答复与撤回。
#pragma once
#include "political_military_appointments.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
namespace hanmo::resident::political_military_detail {
struct AppointmentAudit {
    std::map<StableId,const PoliticalMilitaryAuthorization*> authorizations;
    std::map<StableId,const PoliticalMilitaryAnswer*> answers;
    std::map<StableId,const PoliticalMilitaryWithdrawal*> withdrawals;
};
bool same_identity(const WorldIdentity&,const WorldIdentity&);
bool same_target(const MilitaryOfficerAssignmentTarget&,const MilitaryUnitCommandObservation&);
bool grant_covers(const Grant&,const PoliticalMilitaryAppointmentPolicy&,Integer);
Outcome<StableId> current_authority(const PoliticalMilitaryAppointmentContext&,const ActingAuthority&,const PoliticalMilitaryAppointmentPolicy&);
Status historical_authority(const PoliticalMilitaryAppointmentContext&,const ActingAuthority&,const StableId&,const PoliticalMilitaryAppointmentPolicy&,Integer,Integer);
Status current_source(const PoliticalMilitaryAppointmentContext&,const MilitaryOfficerAssignmentTarget&);
Status historical_source(const PoliticalMilitaryAppointmentContext&,const MilitaryOfficerAssignmentTarget&,Integer,const StableId&);
Outcome<AppointmentAudit> audit(const PoliticalMilitaryAppointmentContext&);
}
