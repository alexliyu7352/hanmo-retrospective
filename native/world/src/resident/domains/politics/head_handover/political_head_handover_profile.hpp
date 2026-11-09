// 生前交接只按独立module版本选择，不占原32位feature或宽化旧PoliticsEffects授权。
#pragma once
#include "political_head_handover_business.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
Outcome<Integer> political_head_handover_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Outcome<std::optional<PoliticalHeadHandoverCatalog>> decode_political_head_handover_profile(const BoundaryValue&,const std::vector<StableId>&);
Status validate_political_head_handover_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PoliticalHeadHandoverState*);
Status validate_political_head_handover_fact_header(const CommonStore&,const PoliticalHeadHandoverState&,const FactRecord&);
bool is_political_head_handover_fact_kind(const StableId&);
// 明细只允许该命令对应的规范来源ID；真正任职/永久结果由完整本账冷验。
bool valid_political_head_handover_effect(const StableId&,const PoliticsEffects&);
}
