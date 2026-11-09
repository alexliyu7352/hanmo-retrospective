// 补任只接原政治事业的显式岗位名单与本人接受范围；不新增Director枚举或默认同意。
#pragma once
#include "political_subordinate_appointment_contract.hpp"
#include "resident/domains/politics/politics_decision_model.hpp"
namespace hanmo::resident {
inline constexpr const char* political_subordinate_appointment_intents_module="political_subordinate_appointment_intents";
// 每项限定原首职本人、候选本人及两条原Director policy，今日换任不会改写作者当事人。
struct PoliticalSubordinateAppointmentIntent {
 StableId id,appointment_policy_id,issuer_actor_id,candidate_actor_id,issuer_director_policy_id,candidate_director_policy_id;
};
struct PoliticalSubordinateAppointmentIntentCatalog {Integer version=1;OrderedTable<PoliticalSubordinateAppointmentIntent> intents;};
// 只借不可变Director原PoliticalCareerPolicy。空或未选原模板由World适配器拒绝。
using PoliticalSubordinateCareerPolicyRead=std::function<const PoliticalCareerPolicy*(const StableId& director_policy_id)>;
Outcome<PoliticalSubordinateAppointmentIntentCatalog> decode_political_subordinate_appointment_intents(const BoundaryValue&);
BoundaryValue encode_political_subordinate_appointment_intents(const PoliticalSubordinateAppointmentIntentCatalog&);
Status validate_political_subordinate_appointment_intents(const PoliticalSubordinateAppointmentIntentCatalog&,const CommonCatalog&,const PoliticsCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalSubordinateAppointmentCatalog&,const PoliticalSubordinateCareerPolicyRead&);
// 对真实原offer当面已知后的本人答复，只按既有事业response与范围，不从预算推意愿。
Outcome<bool> read_political_subordinate_career_response(const PoliticalCareerPolicy&,const StableId& organization,const StableId& jurisdiction);
}
