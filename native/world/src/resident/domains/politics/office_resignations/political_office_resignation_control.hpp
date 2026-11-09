// 本域历史本人只借原Family真实换控链；原职/辖区范围读不反调Task或完整政治总验。
#pragma once
#include "political_office_resignation_contract.hpp"
#include "resident/domains/politics/succession/political_succession_control.hpp"
namespace hanmo::resident {
Outcome<StableId> read_political_office_resignation_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer,const StableId& actual_fact);
struct PoliticalOfficeResignationTerritoryContext {
 const CommonStore& common;const PoliticsState& politics;const PoliticsCatalog& catalog;const PoliticalSuccessionCatalog& successions;
 const PoliticalSuccessionOccupationContext* occupations=nullptr;
};
// 无boundary仅允许current；有boundary必须本域实际申请/答复。发生过辖区接管要求另有显式制度。
Status validate_political_office_resignation_seat(const PoliticalOfficeResignationTerritoryContext&,const PoliticalOfficeResignationPolicy&,Integer,const StableId& actual_fact={});
}
