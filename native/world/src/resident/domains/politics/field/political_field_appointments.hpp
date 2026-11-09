// 野外伤亡后的专项签受独立成段；只证明合法军令，实际军官、舟舰绑定与兵员仍由S2交接。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/military/military_field_command_contract.hpp"
#include <functional>
namespace hanmo::resident {
inline constexpr const char* political_field_issue_kind="political_field_authorized";
inline constexpr const char* political_field_accept_kind="political_field_accepted";
inline constexpr const char* political_field_decline_kind="political_field_declined";
inline constexpr const char* political_field_withdraw_kind="political_field_withdrawn";
inline constexpr const char* political_field_unit_grant="political_assign_field_unit";
inline constexpr const char* political_field_army_grant="political_assign_field_army";
struct PoliticalFieldAppointmentPolicy {
    StableId id,organization_id,origin_garrison_id,origin_site_id;
    Integer response_ticks=2,arrival_ticks=4;
    bool allow_unit=true,allow_army=false;
};
struct PoliticalFieldAppointmentCatalog {
    Integer version=1;bool enabled=false;
    OrderedTable<PoliticalFieldAppointmentPolicy> policies;
};
// 原形成、精确亡者来源和现指挥来源全部冻结；第二步整军接替不能复用第一步部队签受。
struct PoliticalFieldAuthorization {
    StableId id,policy_id,grant_id;
    ActingAuthority authority;
    MilitaryFieldCommandTarget target;
    Integer tick=0,answer_before_tick=0;
};
struct PoliticalFieldAnswer {
    StableId id,authorization_fact_id,actor_id;
    bool accepted=false;
    std::vector<StableId> witness_actor_ids;
    Integer tick=0,apply_before_tick=0;
};
struct PoliticalFieldWithdrawal {
    StableId id,authorization_fact_id,grant_id;
    ActingAuthority authority;
    std::vector<StableId> witness_actor_ids;
    Integer tick=0;
};
struct PoliticalFieldAppointmentState {
    Integer version=1;
    OrderedTable<PoliticalFieldAuthorization> authorizations;
    OrderedTable<PoliticalFieldAnswer> answers;
    OrderedTable<PoliticalFieldWithdrawal> withdrawals;
};
struct PoliticalFieldIssueCommand {ActingAuthority authority;StableId policy_id;MilitaryFieldCommandTarget target;};
struct PoliticalFieldAnswerCommand {StableId authorization_fact_id;bool accept=false;};
struct PoliticalFieldWithdrawCommand {ActingAuthority authority;StableId authorization_fact_id;};
using PoliticalFieldAppointmentCommand=std::variant<PoliticalFieldIssueCommand,PoliticalFieldAnswerCommand,PoliticalFieldWithdrawCommand>;
struct PoliticalFieldAppointmentQuote {WorldIdentity identity;StableId actor_id,kind;PoliticalFieldAppointmentCommand command;};
// 可见完成通知来自S2实际交接记录，签受成功本身不代表军官已经更换。
struct PoliticalFieldApplicationNotice {StableId id,authorization_fact_id,acceptance_fact_id,actor_id;Integer tick=0;};
// 接触证明必须来自同军真实成员与原舟舰binding；政治域不能按相同世界坐标推断能面授军令。
struct PoliticalFieldMilitaryRead {
    std::function<Outcome<MilitaryFieldCommandObservation>(const MilitaryFieldCommandTarget&)> current;
    std::function<Outcome<MilitaryFieldCommandObservation>(const MilitaryFieldCommandTarget&,Integer,const StableId&)> before_fact;
    std::function<Outcome<MilitaryFieldCommandContact>(const MilitaryFieldCommandTarget&,const StableId&)> contact;
    std::function<Outcome<MilitaryFieldCommandContact>(const MilitaryFieldCommandTarget&,const StableId&,Integer,const StableId&)> contact_before_fact;
    std::function<std::optional<StableId>(const StableId&)> garrison_site;
    // 枚举仅限观察者当前真实军团；UI不可从全世界人物或远方军团补候选。
    std::function<Outcome<std::vector<MilitaryFieldCommandTarget>>(const StableId&)> targets;
    std::function<Outcome<std::optional<PoliticalFieldApplicationNotice>>(const StableId&)> application;
    std::function<Status(const StableId&,const StableId&,const StableId&)> application_preview;
    std::function<Outcome<StableId>(const StableId&)> unit_name;
};
struct PoliticalFieldAppointmentContext {
    const CommonStore& common;const CommonCatalog& common_catalog;
    const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
    const LocalEconomyState& economy;const ActivityRead& activity;
    const PoliticalFieldAppointmentCatalog& catalog;const PoliticalFieldAppointmentState& state;
    const PoliticalFieldMilitaryRead& military;
};
Status validate_political_field_catalog(const CommonCatalog&,const OrganizationCatalog&,const PoliticalFieldAppointmentCatalog&,const PoliticalFieldMilitaryRead&);
Status validate_political_field_appointments(const PoliticalFieldAppointmentContext&);
Outcome<PoliticalFieldAppointmentQuote> preview_political_field_appointment(const PoliticalFieldAppointmentContext&,const StableId&,const PoliticalFieldAppointmentCommand&);
Outcome<StableId> apply_political_field_appointment_in_transaction(CommonStore&,PoliticalFieldAppointmentState&,const PoliticalFieldAppointmentContext&,const StableId&,const PoliticalFieldAppointmentCommand&,const PoliticalFieldAppointmentQuote&);
class PoliticalFieldAppointmentAccess final:public MilitaryFieldCommandAppointmentRead {
public:
    explicit PoliticalFieldAppointmentAccess(const PoliticalFieldAppointmentContext& c):context_(c){}
    Outcome<MilitaryFieldCommandAppointmentProof> appointment_for_application(const StableId&,const StableId&,const StableId&)const override;
    Outcome<MilitaryFieldCommandAppointmentProof> appointment_before_application(const StableId&,const StableId&,const StableId&,Integer)const override;
private:const PoliticalFieldAppointmentContext& context_;
};
StableId political_field_appointment_command_kind(const PoliticalFieldAppointmentCommand&);
BoundaryValue encode_political_field_appointment_command(const PoliticalFieldAppointmentCommand&);
Outcome<PoliticalFieldAppointmentCommand> decode_political_field_appointment_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_political_field_catalog(const PoliticalFieldAppointmentCatalog&);
Outcome<PoliticalFieldAppointmentCatalog> decode_political_field_catalog(const BoundaryValue&);
BoundaryValue encode_political_field_appointments(const PoliticalFieldAppointmentState&);
Outcome<PoliticalFieldAppointmentState> decode_political_field_appointments(const BoundaryValue&);
Status visit_political_field_appointments_checkpoint(CodecVisitor&,PoliticalFieldAppointmentState&);
}
