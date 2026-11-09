// 原职位本人到原柜台接任，只更新Common岗位与本段永久事实；不改任何旧职责/私约主体。
#pragma once
#include "political_office_reception_contract.hpp"
namespace hanmo::resident {
// 所有只读依赖必须同候选活到调用结束；座所与历史本人缺源即拒绝。
struct PoliticalOfficeReceptionContext {
 const CommonStore& common;const CommonCatalog& common_catalog;const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;const PoliticalSuccessionCatalog& successions;const PoliticalOfficeReceptionCatalog& catalog;const PoliticalOfficeReceptionState& state;
 FamilyDeathOriginRead death_origin;PoliticalSuccessionSeatRead seat;PoliticalSuccessionPlayerRead player_before;
};
struct PoliticalOfficeReceptionQuote {WorldIdentity identity;StableId actor_id;PoliticalAssumeOfficeReception command;};
Status validate_political_office_reception_sources(const PoliticalOfficeReceptionContext&);
Outcome<PoliticalOfficeReceptionQuote> preview_political_office_reception(const PoliticalOfficeReceptionContext&,const StableId& actor,const PoliticalAssumeOfficeReception&);
// 外层同一候选保存点覆盖Common Role/fact/计数与本账，任一晚错误不得留下部分移交。
Outcome<StableId> apply_political_office_reception_in_candidate(CommonStore&,PoliticalOfficeReceptionState&,const PoliticalOfficeReceptionContext&,const StableId&,const PoliticalAssumeOfficeReception&,const PoliticalOfficeReceptionQuote&);
}
