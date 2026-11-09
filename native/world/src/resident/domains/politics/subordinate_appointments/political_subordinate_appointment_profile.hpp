// 独立有序module选择新补任尾和收据上下文；不占已满feature位或放宽旧政治效果。
#pragma once
#include "political_subordinate_appointment_business.hpp"
namespace hanmo::resident {
Outcome<Integer> political_subordinate_appointment_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Outcome<std::optional<PoliticalSubordinateAppointmentCatalog>> decode_political_subordinate_appointment_profile(const BoundaryValue&,const std::vector<StableId>&);
Outcome<std::optional<PoliticalSubordinateAppointmentIntentCatalog>> decode_political_subordinate_intent_profile(const BoundaryValue&,const std::vector<StableId>&);
Status validate_political_subordinate_appointment_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PoliticalSubordinateAppointmentState*);
Status validate_political_subordinate_appointment_fact_header(const CommonStore&,const PoliticalSubordinateAppointmentState&,const FactRecord&);
bool is_political_subordinate_appointment_fact_kind(const StableId&);
bool political_subordinate_appointment_effect_uses_sources(const PoliticsEffects&);
bool valid_political_subordinate_appointment_effect(const StableId&,const PoliticsEffects&);
}
