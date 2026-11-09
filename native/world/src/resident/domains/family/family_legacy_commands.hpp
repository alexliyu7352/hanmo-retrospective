// 遗留控制的三个具名命令；仅activation能在确认死亡后进入控制选择，其他仍是活人会面。
#pragma once
#include "family_legacy.hpp"
namespace hanmo::resident {
struct FamilyLegacyCommand { StableId kind,request_id,consent_fact_id; };
struct FamilyLegacyQuote { std::optional<LegacyControlDecisionQuote> decision;std::optional<FamilyLegacyActivationQuote> activation; };
// 参数严格区分接受、撤回和激活，旧Family任意参数不能转为死后权利。
Outcome<FamilyLegacyCommand> decode_family_legacy_command(const StableId&,const BoundaryValue&);
Outcome<FamilyLegacyQuote> preview_family_legacy_command(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const LegacyControlConsentCatalog&,const FamilyLegacyState&,const FamilyLegacyPlayableRead&,const StableId& actor,const FamilyLegacyCommand&,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
// 同一候选中再次核对具名报价后执行；FamilyEffect只是原有限回执，无遗产字段。
Outcome<FamilyEffect> apply_family_legacy_command(CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const LegacyControlConsentCatalog&,FamilyLegacyState&,const FamilyLegacyPlayableRead&,const StableId& actor,const FamilyLegacyCommand&,const FamilyLegacyQuote&,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr,OriginalContinuationConsentWrite* continuation_writer=nullptr);
// 只展示当前人物已接触的专用节点和本人参与的许可，每页16项，无全局死亡名单。
Outcome<BoundaryValue> query_family_legacy_page(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const LegacyControlConsentCatalog&,const FamilyLegacyState&,const FamilyLegacyPlayableRead&,Integer page,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
}
