// 独立module选择接待岗位来源，不占已满32位，也不默认放宽旧PoliticsEffects类别。
#pragma once
#include "political_office_reception_business.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
Outcome<Integer> political_office_reception_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Outcome<std::optional<PoliticalOfficeReceptionCatalog>> decode_political_office_reception_profile(const BoundaryValue&,const std::vector<StableId>&);
Status validate_political_office_reception_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PoliticalOfficeReceptionState*);
Status validate_political_office_reception_fact_header(const CommonStore&,const PoliticalOfficeReceptionState&,const FactRecord&);
bool is_political_office_reception_fact_kind(const StableId&);
bool valid_political_office_reception_effect(const StableId&,const PoliticsEffects&);
}
