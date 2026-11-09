// 本人请求和上级答复分别保存真实来源；原Office仍为唯一任职账，批准后原义务由各域自己的连续性规则处理。
#pragma once
#include "political_office_resignation_authored.hpp"
namespace hanmo::resident {
using PoliticalOfficeResignationSeatRead=std::function<Status(const PoliticalOfficeResignationPolicy&,Integer,const StableId&)>;
struct PoliticalOfficeResignationContext {
 const CommonStore& common;const CommonCatalog& common_catalog;const PoliticsState& politics;const PoliticsCatalog& politics_catalog;
 const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;const LocalEconomyState& economy;const ActivityRead& activity;
 const PoliticalSuccessionCatalog& successions;const PoliticalOfficeResignationCatalog& catalog;const PoliticalOfficeResignationState& state;
 FamilyDeathOriginRead death_origin;PoliticalOfficeResignationSeatRead seat;PoliticalSuccessionPlayerRead player_before;
 const PoliticalOfficeResignationResponseCatalog* responses=nullptr;
 const PoliticalOfficeResignationAuthoredOrigin* authored_command=nullptr;
};
struct PoliticalOfficeResignationQuote {WorldIdentity identity;StableId actor_id;PoliticalOfficeResignationCommand command;};
Status validate_political_office_resignation_sources(const PoliticalOfficeResignationContext&);
Outcome<PoliticalOfficeResignationQuote> preview_political_office_resignation(const PoliticalOfficeResignationContext&,const StableId&,const PoliticalOfficeResignationCommand&);
// 外层保存点覆盖Common计数/事实、原Organization和本段；只有成功批准调用原invalidate_office。
Outcome<StableId> apply_political_office_resignation_in_candidate(CommonStore&,OrganizationState&,PoliticalOfficeResignationState&,const PoliticalOfficeResignationContext&,const StableId&,const PoliticalOfficeResignationCommand&,const PoliticalOfficeResignationQuote&);
}
