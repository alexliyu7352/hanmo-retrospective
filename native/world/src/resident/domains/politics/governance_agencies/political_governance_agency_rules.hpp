// 本域只借同候选原治理/公职/人物来源，独立代理不成为平行Office或预算账。
#pragma once
#include "political_governance_agency_contract.hpp"
namespace hanmo::resident {
struct PoliticalGovernanceAgencyIntentCatalog;
using PoliticalGovernanceAgencySeatRead=std::function<Status(const PoliticalGovernanceAgencyPolicy&,Integer,const StableId&)>;
struct PoliticalGovernanceAgencyContext {
 const CommonStore& common;const CommonCatalog& common_catalog;const PoliticsState& politics;const PoliticsCatalog& politics_catalog;
 const PoliticsGovernanceState& governance;const PoliticsGovernanceCatalog& governance_catalog;const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;const PoliticalGovernanceAgencyCatalog& catalog;const PoliticalGovernanceAgencyState& state;
 FamilyDeathOriginRead death_origin;PoliticalGovernanceAgencySeatRead seat;PoliticalSuccessionPlayerRead player_before;
 // 只让既有签字之后的真实单步生命/占领源到to，所有本业务签字tick仍<=Common。
 const DynamicStepInput* phase=nullptr;
 const PoliticalGovernanceAgencyIntentCatalog* intents=nullptr;
 // 只由实际Director采用口同栈传入；玩家调用必须为空，不能从客户端参数选择来源。
 const PoliticalGovernanceAgencyAuthoredOrigin* authored_command=nullptr;
};
struct PoliticalGovernanceAgencyQuote {WorldIdentity identity;StableId actor_id;PoliticalGovernanceAgencyCommand command;};
// 此先验可在本次真实result分配前调用；永久提交回执由外层业务全验另核。
Status validate_political_governance_agency_sources(const PoliticalGovernanceAgencyContext&);
Outcome<PoliticalGovernanceAgencyQuote> preview_political_governance_agency(const PoliticalGovernanceAgencyContext&,const StableId&,const PoliticalGovernanceAgencyCommand&);
// 首段委任/答复/撤回只写Common和本账；代理实际发行另走显式原治理唯一writer。
Outcome<StableId> apply_political_governance_agency_mandate_in_candidate(CommonStore&,PoliticalGovernanceAgencyState&,const PoliticalGovernanceAgencyContext&,const StableId&,const PoliticalGovernanceAgencyCommand&,const PoliticalGovernanceAgencyQuote&);
// 仅代理实际签发写原Governance和本账，不重建Task、公共资源或Office。
Outcome<StableId> apply_political_governance_agency_issue_in_candidate(CommonStore&,PoliticsGovernanceState&,PoliticalGovernanceAgencyState&,const PoliticalGovernanceAgencyContext&,const StableId&,const PoliticalIssueAgencyGovernance&,const PoliticalGovernanceAgencyQuote&);
}
