// 复用原Director公平复评与事务；这层只产生一个已实际报价的零时长委任命令。
#pragma once
#include "diplomacy_response_director.hpp"
#include "diplomacy_delegation_npc.hpp"
namespace hanmo::resident {
Status validate_diplomatic_delegation_director_catalog(const DirectorCatalog&,const OrganizationCatalog&,const DiplomacyCatalog*);
Status validate_diplomatic_delegation_director_history(const DirectorState&,const DirectorCatalog&,const DiplomacyState*);
Outcome<bool> diplomatic_delegation_available(const DirectorDiplomaticContext&,const StableId& actor,const StableId& policy_id);
Outcome<std::optional<DecisionProposal>> propose_director_diplomatic_delegation(
    const DirectorDiplomaticContext&,const KnownSubjectView&,const StableId& policy_id,const DecisionProposal& base);
}
