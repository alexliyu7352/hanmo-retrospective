// 生前本人声明只授权自己的控制接续；亲生关系、真实会面与原许可决定分别可追溯。
#pragma once
#include "biological_control_catalog.hpp"
namespace hanmo::resident {
struct BiologicalControlDecisionContext {
    BiologicalLegacyBasisContext people;
    const BiologicalControlCatalog& catalog;
    const LegacyControlConsentCatalog& original_requests;
    const InteractionCatalog& interaction_catalog;
    BiologicalControlTemplateRead templates;
    const BornPersonalDialogueRead* born_dialogues=nullptr;
};
// Birth/Creation/Lineage/Life先验 → Born原打开表及纯控制折叠 → 本源/Legacy完整验证；不反入World。
Status validate_biological_control_context(const BiologicalControlDecisionContext&);
Status validate_biological_control_decision(const BiologicalControlDecisionContext&,const LegacyControlDecisionSource&,const StableId& actual_legacy_fact={});
Outcome<LegacyControlDecisionQuote> preview_biological_control_decision(const BiologicalControlDecisionContext&,const InteractionState&,const InteractionAccess&,const StableId& predecessor,const StableId& successor,const StableId& request,const StableId& decision);
Status revalidate_biological_control_decision(const BiologicalControlDecisionContext&,const InteractionState&,const InteractionAccess&,const LegacyControlDecisionQuote&);
}
