// 原Director借此读取当前候选，索引每次重建，不持有发布页或上一NPC的旧快照。
#pragma once
#include "diplomacy_response_policy.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
struct TreatyPassageContext;
struct DirectorDiplomaticContext {
    const DiplomaticPresentRead& present;
    const CommonCatalog& common_catalog;
    const ActivityRead& activity;
    const TreatyPassageContext* treaty=nullptr;
};
Status validate_diplomatic_director_catalog(const DirectorCatalog&,const OrganizationCatalog&,const DiplomacyCatalog*,const std::vector<std::pair<StableId,StableId>>& module_versions);
Status validate_diplomatic_director_history(const DirectorState&,const DirectorCatalog&,const DiplomacyState*);
Outcome<bool> diplomatic_response_available(const DirectorDiplomaticContext&,const StableId& actor,const DiplomaticResponsePolicy&,Integer candidate_limit);
Outcome<std::optional<DecisionProposal>> propose_director_diplomatic_response(const DirectorDiplomaticContext&,const DirectorCatalog&,const KnownSubjectView&,const DiplomaticResponsePolicy&,const DecisionProposal& base);
}
