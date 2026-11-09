// 自愿接续持有本域同意、撤回与唯一交接事实；资源仍只经CommonStore结算。
#pragma once
#include "family_model.hpp"
namespace hanmo::resident {
// 仅显式新Family25..32早门可使用；不得以此替代Known/关系之后的完整本人来源末验。
Status validate_voluntary_succession_structure(const CommonStore&,const FamilyState&);
// 所有长期来源、唯一终态与索引必须一致；没有启用新profile时明确拒绝新事实。
Status validate_voluntary_succession_facts(const CommonStore&,const FamilyState&,const CommonCatalog&,const InteractionCatalog*,const SuccessionConsentCatalog*,const FamilyLegacyState* legacy=nullptr);
// 两代本人同意只生成待办计划，不扣钱、不锁货，也不切换玩家。
Outcome<StableId> accept_succession_plan(CommonStore&,FamilyState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const SuccessionConsentCatalog&,const SuccessionConsentQuote&,const FamilyLegacyState* legacy=nullptr);
// Borrowed final-row construction inputs for the sole K9 raw copy planner.
// The original writer preserves empty field_order; otherwise appends only its
// existing conclusion field, in a fresh final-sized vector before table set.
struct SuccessionPlanCancellationBuildView {
 const SuccessionConsentFact& original;Integer cancelled_tick;
};
// 取消只由原当前玩家执行；已交割不能借取消回滚整个世界或收回继任者的钱货。
Status cancel_succession_plan(CommonStore&,FamilyState&,const StableId& actor,const StableId& plan_id);
// 提交前精确重报价，已同意的原批被消费/转移后须重新协商，不能偷换为另一批。
Outcome<SuccessionPersonalQuote> preview_accepted_succession_assets(const CommonStore&,const FamilyState&,const CommonCatalog&,const InteractionCatalog&,const SuccessionConsentCatalog&,const StableId& plan_id,const FamilyLegacyState* legacy=nullptr);
// 调用者仍须核两人稳定可用、会话已关闭及未结责任；本口沿同候选原子结算及换人。
Outcome<StableId> complete_succession_plan(CommonStore&,FamilyState&,const CommonCatalog&,const InteractionCatalog&,const SuccessionConsentCatalog&,const StableId& plan_id,const SuccessionPersonalQuote&,const FamilyLegacyState* legacy=nullptr);
// 原旧FamilyChoice与新显式计划共享同一有序控制链，缺来源明确拒绝。
Outcome<FamilySuccessionStep> family_succession_step(const FamilyState&,const StableId& fact_id);
// 只展示当前人物已到达的明确同意来源及本人参与的计划；不导出其他家庭的私产安排。
Outcome<BoundaryValue> query_voluntary_succession_page(const CommonStore&,const FamilyState&,const FamilyCatalog&,const CommonCatalog&,const EstateState&,const EstateCatalog&,const FamilyAccess&,Integer page);
}
