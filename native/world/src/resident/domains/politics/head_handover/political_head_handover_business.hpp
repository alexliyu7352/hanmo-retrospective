// 公共办理先完整核原三账及实际提交来源，避免低层临时未绑定行成为下一请求的许可。
#pragma once
#include "political_head_handover_receipts.hpp"
namespace hanmo::resident {
struct PoliticalHeadHandoverBusinessContext {const PoliticalHeadHandoverContext& rules;PoliticalHeadHandoverResultMatch result_match;};
struct PoliticalHeadHandoverEffects {StableId source_fact_id,office_change_fact_id;};
Status validate_political_head_handovers(const PoliticalHeadHandoverBusinessContext&);
Outcome<PoliticalHeadHandoverQuote> preview_political_head_handover_business(const PoliticalHeadHandoverBusinessContext&,const StableId&,const PoliticalHeadHandoverCommand&);
// 回执绑定在真正Common提交尾；返回值只携本次源和可选原Office结果，不另造公权状态。
Outcome<PoliticalHeadHandoverEffects> apply_political_head_handover_business_in_candidate(CommonStore&,OrganizationState&,PoliticalHeadHandoverState&,const PoliticalHeadHandoverBusinessContext&,const StableId&,const PoliticalHeadHandoverCommand&,const PoliticalHeadHandoverQuote&,OrganizationRuntime* exact_runtime=nullptr);
}
