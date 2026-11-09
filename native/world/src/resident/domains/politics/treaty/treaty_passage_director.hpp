// NPC签署借原Director公平复评；目录明示模板与具体公职认可缺一不可。
#pragma once
#include "treaty_passage_command.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_treaty_passage(const TreatyPassageContext&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Status validate_treaty_passage_director(const DirectorCatalog&,const DirectorState&,const TreatyPassageCatalog*,const TreatyPassageState*,const OrganizationCatalog&);
}
