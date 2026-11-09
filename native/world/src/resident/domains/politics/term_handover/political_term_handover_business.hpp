// 公共办理先完整核原三账及实际提交来源，避免低层临时未绑定行成为下一请求的许可。
#pragma once
#include "political_term_handover_receipts.hpp"
namespace hanmo::resident {
struct PoliticalTermHandoverBusinessContext {const PoliticalTermHandoverContext& rules;PoliticalTermHandoverResultMatch result_match;};
struct PoliticalTermHandoverEffects {StableId source_fact_id,office_change_fact_id;};
Status validate_political_term_handovers(const PoliticalTermHandoverBusinessContext&);
Outcome<PoliticalTermHandoverQuote> preview_political_term_handover_business(const PoliticalTermHandoverBusinessContext&,const StableId&,const PoliticalTermHandoverCommand&);
// 回执绑定在真正Common提交尾；返回值只携本次源和可选原Office结果，不另造公权状态。
Outcome<PoliticalTermHandoverEffects> apply_political_term_handover_business_in_candidate(CommonStore&,OrganizationState&,PoliticalTermHandoverState&,const PoliticalTermHandoverBusinessContext&,const StableId&,const PoliticalTermHandoverCommand&,const PoliticalTermHandoverQuote&);
}
