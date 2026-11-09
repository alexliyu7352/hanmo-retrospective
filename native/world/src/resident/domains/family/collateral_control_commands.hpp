// 本人先确认对方真实的旁支认可声明，再另行接受或撤回自己的条件控制许可。
#pragma once
#include "collateral_control_receipts.hpp"
#include "family_legacy_commands.hpp"
namespace hanmo::resident {
struct CollateralControlCommand {StableId kind,request_id,candidate_actor_id,recognition_fact_id,consent_fact_id;};
struct CollateralControlQuote {std::optional<CollateralRecognitionSource> recognition;std::optional<LegacyControlDecisionQuote> decision;};
Outcome<CollateralControlCommand> decode_collateral_control_command(const StableId&,const BoundaryValue&);
Outcome<CollateralControlQuote> preview_collateral_control_command(const CollateralControlDecisionContext&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionAccess&,const FamilyLegacyState&,const FamilyLegacyPlayableRead&,const StableId& actor,const CollateralControlCommand&,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr);
// 玩家World完整候选入口；原Common与本域保存点回滚同对象，NPC/Director必须在写前拒绝本命令。
Outcome<FamilyEffect> apply_collateral_control_command(CommonStore&,const CollateralControlDecisionContext&,CollateralControlState&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionAccess&,FamilyLegacyState&,const FamilyLegacyPlayableRead&,const StableId& actor,const CollateralControlCommand&,const CollateralControlQuote&,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr);
// 每页16，只有当前真实私人SAY与本人已参加的永久认可/许可；不按隐藏族谱产生可见候选。
Outcome<BoundaryValue> query_collateral_control_page(const CollateralControlDecisionContext&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionAccess&,const FamilyLegacyState&,const FamilyLegacyPlayableRead&,Integer page,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr);
}
