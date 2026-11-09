// 已认可旁支的另一份本人条件许可；血缘、认可与控制同意三种来源彼此不能替代。
#pragma once
#include "collateral_control_state.hpp"
namespace hanmo::resident {
struct CollateralControlDecisionContext {
    const CollateralRecognitionContext& recognition;
    const CollateralControlState& state;
};
// 完整认可/actual-result与Known opening先验，不反入整个FamilyLegacy或World。
Status validate_collateral_control_context(const CollateralControlDecisionContext&);
Status validate_collateral_control_decision(const CollateralControlDecisionContext&,const LegacyControlDecisionSource&,const StableId& actual_legacy_fact={});
// 原认可ID必须由这次显式命令指定，不用最近一条认可或同姓关系兜底。
Outcome<LegacyControlDecisionQuote> preview_collateral_control_decision(const CollateralControlDecisionContext&,const StableId& predecessor,const StableId& successor,const StableId& request,const StableId& decision,const StableId& exact_recognition);
Status revalidate_collateral_control_decision(const CollateralControlDecisionContext&,const LegacyControlDecisionQuote&);
}
