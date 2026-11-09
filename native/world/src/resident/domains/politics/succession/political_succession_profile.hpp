// 显式module/domain选择独立新尾；不占原32位feature、不扩大旧政治Done授权。
#pragma once
#include "political_succession_business.hpp"
namespace hanmo::resident {
Outcome<Integer> political_succession_selected_version(const std::vector<std::pair<StableId,StableId>>& modules);
Outcome<std::optional<PoliticalSuccessionCatalog>> decode_political_succession_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_political_succession_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PoliticalSuccessionState*);
Status validate_political_succession_fact_header(const CommonStore&,const PoliticalSuccessionState&,const FactRecord&);
bool is_political_succession_fact_kind(const StableId&);
}
