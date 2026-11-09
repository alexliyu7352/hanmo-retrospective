// Original S6 mutation and original S5 interval adoption share one unpublished
// candidate. This owns no production state or second responsibility history.
#pragma once
#include "politics_takeover_rules.hpp"
#include "resident/domains/local_economy/production/production_responsibility_change.hpp"
namespace hanmo::resident {
struct PoliticalTakeoverProductionWriteContext {
 const LocalEconomyState& economy;
 const LocalEconomyCatalog& economy_catalog;
 EconomicProductionContext& production;
 const EconomicProductionAdoptionRead& adoption;
};
Outcome<std::optional<PreparedProductionResponsibilityChange>> prepare_political_production_change(
 const CommonStore&,const std::vector<PoliticalOriginalResponsibility>&,
 const WorldMoment&,PoliticalTakeoverProductionWriteContext*);
Status apply_political_production_change_in_candidate(CommonStore&,const PoliticalTakeoverState&,
 std::optional<PreparedProductionResponsibilityChange>&&,const MomentOwnerSource&,
 PoliticalTakeoverProductionWriteContext*);
}
