// 补任纯读和原Office写者的同候选合同；缺身份、管辖或资格历史即不可用。
#pragma once
#include "political_subordinate_appointment_codec.hpp"
#include "political_subordinate_appointment_authored.hpp"
namespace hanmo::resident {
using PoliticalSubordinateSeatRead=std::function<Status(const PoliticalSubordinateAppointmentPolicy&,Integer,const StableId&)>;
struct PoliticalSubordinateAppointmentContext {
 const CommonStore& common;const CommonCatalog& common_catalog;const PoliticsState& politics;const PoliticsCatalog& politics_catalog;
 const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;const LocalEconomyState& economy;const ActivityRead& activity;
 const PoliticalSuccessionCatalog& heads;const PoliticalSubordinateAppointmentCatalog& catalog;const PoliticalSubordinateAppointmentState& state;
 FamilyDeathOriginRead death_origin;PoliticalSubordinateSeatRead seats;PoliticalSuccessionPlayerRead player_before;
 const PoliticalSubordinateQualificationRead* qualification=nullptr;
 const PoliticalSubordinateAppointmentIntentCatalog* intents=nullptr;PoliticalSubordinateCareerPolicyRead career;
 // 仅内部原Director包装命令借本次origin，外部decode从不提供此指针。
 const PoliticalSubordinateAppointmentAuthoredOrigin* authored=nullptr;
};
struct PoliticalSubordinateAppointmentQuote {WorldIdentity identity;StableId actor_id;PoliticalSubordinateAppointmentCommand command;};
Status validate_political_subordinate_appointments(const PoliticalSubordinateAppointmentContext&);
Outcome<PoliticalSubordinateAppointmentQuote> preview_political_subordinate_appointment(const PoliticalSubordinateAppointmentContext&,const StableId&,const PoliticalSubordinateAppointmentCommand&);
// 原Common保存点属于调用者；本域/Organization先暂存，任何末失败不发布半次职位变化。
Outcome<StableId> apply_political_subordinate_appointment_in_candidate(CommonStore&,OrganizationState&,PoliticalSubordinateAppointmentState&,const PoliticalSubordinateAppointmentContext&,const StableId&,const PoliticalSubordinateAppointmentCommand&,const PoliticalSubordinateAppointmentQuote&);
}
