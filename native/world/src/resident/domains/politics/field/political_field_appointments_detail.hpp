// 同候选临时审计，不保存第二份指挥权或伤亡史。
#pragma once
#include "political_field_appointments.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
namespace hanmo::resident::political_field_detail {
struct Audit {
    std::map<StableId,const PoliticalFieldAuthorization*> authorizations;
    std::map<StableId,const PoliticalFieldAnswer*> answers;
    std::map<StableId,const PoliticalFieldWithdrawal*> withdrawals;
};
bool identity(const WorldIdentity&,const WorldIdentity&);
bool same_source(const MilitaryFieldCommandSource&,const MilitaryFieldCommandSource&);
bool same_observation(const MilitaryFieldCommandTarget&,const MilitaryFieldCommandObservation&);
bool grant_covers(const Grant&,const PoliticalFieldAppointmentPolicy&,MilitaryFieldCommandKind,Integer);
Outcome<StableId> current_authority(const PoliticalFieldAppointmentContext&,const ActingAuthority&,const PoliticalFieldAppointmentPolicy&,MilitaryFieldCommandKind);
Status historical_authority(const PoliticalFieldAppointmentContext&,const ActingAuthority&,const StableId&,const PoliticalFieldAppointmentPolicy&,MilitaryFieldCommandKind,Integer,Integer);
Status source(const PoliticalFieldAppointmentContext&,const MilitaryFieldCommandTarget&,Integer,const StableId& before={});
Status contact(const PoliticalFieldAppointmentContext&,const MilitaryFieldCommandTarget&,const StableId&,Integer,const StableId& before={});
Outcome<Audit> audit(const PoliticalFieldAppointmentContext&);
}
