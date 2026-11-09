// 原首职对具名下属空缺的任官权；不把本职Grant、家族换控或首职继位当作跨辖区再授予。
#pragma once
#include "resident/domains/politics/succession/political_succession_nominations.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
inline constexpr const char* political_subordinate_appointment_module="political_subordinate_appointments";
inline constexpr const char* political_subordinate_offer_command="political_offer_subordinate_appointment";
inline constexpr const char* political_subordinate_answer_command="political_answer_subordinate_appointment";
inline constexpr const char* political_subordinate_withdraw_command="political_withdraw_subordinate_appointment";
inline constexpr const char* political_subordinate_arrive_command="political_arrive_subordinate_appointment";
// 单一政策绑定原首职、原下属辖区和唯一专项零资产Grant，资格/期限引用原AppointmentPolicy。
struct PoliticalSubordinateAppointmentPolicy {
 StableId id,jurisdiction_id,head_succession_policy_id,appointment_grant_id,appointment_policy_id;
};
struct PoliticalSubordinateAppointmentCatalog {Integer version=1;OrderedTable<PoliticalSubordinateAppointmentPolicy> policies;};
// 首府当面签发不改Office；本人答允后真实赴原辖区，接受和到任均重新核原首职/epoch/Grant。
struct PoliticalOfferSubordinateAppointment {ActingAuthority authority;StableId policy_id,candidate_actor_id;Integer expected_target_epoch=0,expected_jurisdiction_revision=0;};
struct PoliticalAnswerSubordinateAppointment {StableId offer_fact_id;bool accept=false;};
struct PoliticalWithdrawSubordinateAppointment {ActingAuthority authority;StableId offer_fact_id;};
struct PoliticalArriveSubordinateAppointment {StableId acceptance_fact_id;Integer expected_target_epoch=0;};
using PoliticalSubordinateAppointmentCommand=std::variant<PoliticalOfferSubordinateAppointment,PoliticalAnswerSubordinateAppointment,PoliticalWithdrawSubordinateAppointment,PoliticalArriveSubordinateAppointment>;
// 完整业务启用前必须接真实能力历史；今天增长不能倒填旧提名或到任时的最低资格。
struct PoliticalSubordinateQualificationSource {
 WorldIdentity identity;StableId actor_id,boundary_fact_id;Integer observed_tick=0,administration=0,merit=0;
 // 无功勋写者历史时仅证明非负下界0；正门槛必须另有实际完整功绩源。
 bool merit_history_complete=false;
};
class PoliticalSubordinateQualificationRead {
public:
 virtual ~PoliticalSubordinateQualificationRead()=default;
 virtual Outcome<PoliticalSubordinateQualificationSource> subordinate_office_qualification(const StableId& actor_id)const=0;
 virtual Outcome<PoliticalSubordinateQualificationSource> subordinate_office_qualification_before(const StableId& actor_id,Integer tick,const StableId& actual_fact)const=0;
};
// 只投影原Organization完整Office链，不另建持有人/薪资/角色账。
struct PoliticalSubordinateOfficeSource {
 WorldIdentity identity;StableId organization_id,office_id,last_change_fact_id,boundary_fact_id;
 std::optional<StableId> holder_actor_id;Integer office_epoch=0,observed_tick=0;
};
std::optional<Integer> political_subordinate_appointment_fact_sequence(const CommonStore&,const StableId& actual_fact);
Outcome<PoliticalSubordinateOfficeSource> read_political_subordinate_office(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const StableId& office_id);
Outcome<PoliticalSubordinateOfficeSource> read_political_subordinate_office_before(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const StableId& office_id,Integer tick,const StableId& actual_fact);
Status validate_political_subordinate_appointment_catalog(const CommonStore&,const CommonCatalog&,const PoliticsCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalSubordinateAppointmentCatalog&);
Outcome<PoliticalSubordinateAppointmentCatalog> decode_political_subordinate_appointment_catalog(const BoundaryValue&);
BoundaryValue encode_political_subordinate_appointment_catalog(const PoliticalSubordinateAppointmentCatalog&);
} // 新段尚未注册；原politics的delegation_allowed和任命五命令不改变。
