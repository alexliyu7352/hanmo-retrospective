// 動態子代在實際本人會面中簽署；指定實際actor，不讓prototype或request默選另一人。
#pragma once
#include "biological_control_sources.hpp"
#include "family_legacy_commands.hpp"
namespace hanmo::resident {
struct BiologicalControlCommand {StableId kind,request_id,successor_actor_id,consent_fact_id;};
Outcome<BiologicalControlCommand> decode_biological_control_command(const StableId&,const BoundaryValue&);
Outcome<LegacyControlDecisionQuote> preview_biological_control_command(const BiologicalControlDecisionContext&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionAccess&,const FamilyLegacyState&,const FamilyLegacyPlayableRead&,const StableId& actor,const BiologicalControlCommand&,const DiscipleControlRelationContext* disciple=nullptr,const CollateralControlDecisionContext* collateral=nullptr);
Outcome<FamilyEffect> apply_biological_control_command(CommonStore&,const BiologicalControlDecisionContext&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionAccess&,FamilyLegacyState&,const FamilyLegacyPlayableRead&,const StableId& actor,const BiologicalControlCommand&,const LegacyControlDecisionQuote&,const DiscipleControlRelationContext* disciple=nullptr,const CollateralControlDecisionContext* collateral=nullptr);
}
namespace hanmo::resident {
// 仅本人正在进行的真实会面及本人参加的原许可，每页16条；不扫描世界全部亲生人物。
Outcome<BoundaryValue> query_biological_control_page(const BiologicalControlDecisionContext&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionAccess&,const FamilyLegacyState&,const FamilyLegacyPlayableRead&,Integer page,const DiscipleControlRelationContext* disciple=nullptr,const CollateralControlDecisionContext* collateral=nullptr);
}
