// 显式辞任module选择独立尾；已满Common feature位不扩宽，旧PoliticsEffects类别不因本文件自动放行。
#pragma once
#include "political_office_resignation_business.hpp"
namespace hanmo::resident {
Outcome<Integer> political_office_resignation_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Outcome<std::optional<PoliticalOfficeResignationCatalog>> decode_political_office_resignation_profile(const BoundaryValue&,const std::vector<StableId>&);
Status validate_political_office_resignation_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PoliticalOfficeResignationState*);
Status validate_political_office_resignation_fact_header(const CommonStore&,const PoliticalOfficeResignationState&,const FactRecord&);
bool is_political_office_resignation_fact_kind(const StableId&);
bool political_office_resignation_effect_uses_sources(const PoliticsEffects&);
bool valid_political_office_resignation_effect(const StableId&,const PoliticsEffects&);
}
