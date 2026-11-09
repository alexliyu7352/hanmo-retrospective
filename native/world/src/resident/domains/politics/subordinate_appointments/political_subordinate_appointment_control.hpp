// 本域历史本人只借原Family真实换控链；原职/辖区范围读不反调Task或完整政治总验。
#pragma once
#include "political_subordinate_appointment_contract.hpp"
#include "resident/domains/politics/succession/political_succession_control.hpp"
namespace hanmo::resident {
Outcome<StableId> read_political_subordinate_appointment_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer,const StableId& actual_fact);
struct PoliticalSubordinateTerritoryContext {
 const CommonStore& common;const PoliticsState& politics;const PoliticsCatalog& catalog;const PoliticalSuccessionCatalog& heads;
 const PoliticalSuccessionOccupationContext* occupations=nullptr;
};
// 无boundary仅允许current；有boundary必须本域实际提名/答复/到任。发生过辖区接管要求另有显式制度。
Status validate_political_subordinate_appointment_seats(const PoliticalSubordinateTerritoryContext&,const PoliticalSubordinateAppointmentPolicy&,Integer,const StableId& actual_fact={});
}
