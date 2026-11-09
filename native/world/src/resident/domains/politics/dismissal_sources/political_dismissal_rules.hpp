// 原撤职的完整受控本人来源；只包装原Politics/Organization写者，不另授任免或清债。
#pragma once
#include "political_dismissal_receipts.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident {
using PoliticalDismissalSeatRead=std::function<Status(const PoliticalDismissalSourcePolicy&,Integer,const StableId&)>;
using PoliticalDismissalPlayerRead=std::function<Outcome<StableId>(Integer,const StableId&)>;
struct PoliticalDismissalContext {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const PoliticsState& politics;const PoliticsCatalog& politics_catalog;
 const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;
 const PoliticalDismissalSourceCatalog& catalog;const PoliticalDismissalSourceState& state;
 FamilyDeathOriginRead death_origin;PoliticalDismissalSeatRead seats;PoliticalDismissalPlayerRead player_before;
 PoliticalDismissalResultMatch result_matches;
 // Complete selected profile borrows the same candidate runtime and trusted phase clock.
 const PoliticalOrganizationRuntimeWriteContext* organization_runtime=nullptr;
};
struct PoliticalDismissalSourceQuote {
 WorldIdentity identity;PoliticalDismissCommand command;StableId actor_id,policy_id,removed_actor_id;Integer next_instance_id=0;
};
Status validate_political_dismissal_source_catalog(const PoliticsCatalog&,const OrganizationCatalog&,const PoliticalDismissalSourceCatalog&);
// 可信原writer后的临时来源可尚未绑定result；此口只核全部权限/原事实反向覆盖，不能用于稳定存读准入。
Status validate_political_dismissal_sources(const PoliticalDismissalContext&);
// 稳定World/页面/预览必须走完整门，永久结果不得缺漏；依原人物/会员/Office/辖区完整史晚验。
Status validate_political_dismissal_sources_complete(const PoliticalDismissalContext&);
Outcome<PoliticalDismissalSourceQuote> preview_political_dismissal_source(const PoliticalDismissalContext&,const StableId&,const PoliticalDismissCommand&);
// 同候选原子调用原Politics writer；唯一两头成功且权限历史可重放才发布本域/原任职状态。
Outcome<PoliticsEffects> apply_political_dismissal_with_source(CommonStore&,PoliticsState&,OrganizationState&,PoliticalDismissalSourceState&,const PoliticalDismissalContext&,const StableId&,const PoliticalDismissCommand&,const PoliticalDismissalSourceQuote&);
// 完整未发布World候选入口；任意失败由调用方整体丢弃，包含唯一OrganizationRuntime。
Outcome<PoliticsEffects> apply_political_dismissal_with_source_in_candidate(CommonStore&,PoliticsState&,OrganizationState&,PoliticalDismissalSourceState&,const PoliticalDismissalContext&,const StableId&,const PoliticalDismissCommand&,const PoliticalDismissalSourceQuote&);
}
