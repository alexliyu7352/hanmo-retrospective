// 政治只签发与受领具名调任；实际部队指挥、军资和兵员交接仍由军事域唯一办理。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/military/military_officer_contract.hpp"
#include <functional>
namespace hanmo::resident {
inline constexpr const char* political_officer_issue_kind="political_military_officer_authorized";
inline constexpr const char* political_officer_accept_kind="political_military_officer_accepted";
inline constexpr const char* political_officer_decline_kind="political_military_officer_declined";
inline constexpr const char* political_officer_withdraw_kind="political_military_officer_withdrawn";
inline constexpr const char* political_officer_grant_command="political_assign_military_officer";
struct PoliticalMilitaryAppointmentPolicy {
    StableId id,organization_id,garrison_id,site_id;
    Integer response_ticks=4,arrival_ticks=12;
};
struct PoliticalMilitaryAppointmentCatalog {
    Integer version=1;bool enabled=false;
    OrderedTable<PoliticalMilitaryAppointmentPolicy> policies;
};
struct PoliticalMilitaryAuthorization {
    StableId id,policy_id,grant_id,meeting_place_id;
    ActingAuthority authority;
    MilitaryOfficerAssignmentTarget target;
    Integer tick=0,answer_before_tick=0;
};
struct PoliticalMilitaryAnswer {
    StableId id,authorization_fact_id,actor_id;
    bool accepted=false;
    std::vector<StableId> witness_actor_ids;
    Integer tick=0,apply_before_tick=0;
};
struct PoliticalMilitaryWithdrawal {
    StableId id,authorization_fact_id,grant_id;
    std::vector<StableId> witness_actor_ids;
    ActingAuthority authority;
    Integer tick=0;
};
struct PoliticalMilitaryAppointmentState {
    Integer version=1;
    OrderedTable<PoliticalMilitaryAuthorization> authorizations;
    OrderedTable<PoliticalMilitaryAnswer> answers;
    OrderedTable<PoliticalMilitaryWithdrawal> withdrawals;
};
struct PoliticalMilitaryIssueCommand {
    ActingAuthority authority;StableId policy_id;
    MilitaryOfficerAssignmentTarget target;
};
struct PoliticalMilitaryAnswerCommand {StableId authorization_fact_id;bool accept=false;};
struct PoliticalMilitaryWithdrawCommand {ActingAuthority authority;StableId authorization_fact_id;};
using PoliticalMilitaryAppointmentCommand=std::variant<PoliticalMilitaryIssueCommand,PoliticalMilitaryAnswerCommand,PoliticalMilitaryWithdrawCommand>;
struct PoliticalMilitaryApplicationNotice {StableId id,authorization_fact_id,acceptance_fact_id,actor_id;Integer tick=0;};
// 历史观察须由原S2 rosters_before与command_source_before共同生成，不能用今天营地补过去。
struct PoliticalMilitaryAppointmentUnitRead {
    std::function<Outcome<MilitaryUnitCommandObservation>(const StableId&)> current;
    std::function<Outcome<MilitaryUnitCommandObservation>(const StableId&,Integer,const StableId&)> before_fact;
    std::function<std::optional<StableId>(const StableId&)> garrison_site;
    std::function<Outcome<std::vector<MilitaryUnitCommandObservation>>(const StableId&)> at_garrison;
    std::function<Status(const StableId&,const StableId&,const StableId&)> application_preview;
    std::function<Outcome<std::optional<PoliticalMilitaryApplicationNotice>>(const StableId&)> application;
    std::function<Outcome<StableId>(const StableId&)> unit_name;
};
struct PoliticalMilitaryAppointmentContext {
    const CommonStore& common;
    const CommonCatalog& common_catalog;
    const OrganizationState& organizations;
    const OrganizationCatalog& organization_catalog;
    const LocalEconomyState& economy;
    const ActivityRead& activity;
    const PoliticalMilitaryAppointmentCatalog& catalog;
    const PoliticalMilitaryAppointmentState& state;
    const PoliticalMilitaryAppointmentUnitRead& units;
    // 仅Military7显式配对可读取返营后的野外交接来源；旧驻营上下文仍拒新枚举。
    bool field_commands=false;
};
struct PoliticalMilitaryAppointmentQuote {WorldIdentity identity;StableId actor_id,kind;PoliticalMilitaryAppointmentCommand command;};
Status validate_political_military_appointment_catalog(const CommonCatalog&,const OrganizationCatalog&,const PoliticalMilitaryAppointmentCatalog&,const PoliticalMilitaryAppointmentUnitRead&);
Status validate_political_military_appointments(const PoliticalMilitaryAppointmentContext&);
Outcome<PoliticalMilitaryAppointmentQuote> preview_political_military_appointment(const PoliticalMilitaryAppointmentContext&,const StableId&,const PoliticalMilitaryAppointmentCommand&);
// 调用者持共同World候选；本口只写Common和本段，后续任何失败必须整候选撤回。
Outcome<StableId> apply_political_military_appointment_in_transaction(CommonStore&,PoliticalMilitaryAppointmentState&,const PoliticalMilitaryAppointmentContext&,const StableId&,const PoliticalMilitaryAppointmentCommand&,const PoliticalMilitaryAppointmentQuote&);
class PoliticalMilitaryAppointmentAccess final:public MilitaryOfficerAppointmentRead {
public:
    explicit PoliticalMilitaryAppointmentAccess(const PoliticalMilitaryAppointmentContext& context):context_(context){}
    Outcome<MilitaryOfficerAppointmentProof> appointment_for_application(const StableId&,const StableId&,const StableId&)const override;
    Outcome<MilitaryOfficerAppointmentProof> appointment_before_application(const StableId&,const StableId&,const StableId&,Integer)const override;
private:const PoliticalMilitaryAppointmentContext& context_;
};
StableId political_military_appointment_command_kind(const PoliticalMilitaryAppointmentCommand&);
BoundaryValue encode_political_military_appointment_command(const PoliticalMilitaryAppointmentCommand&);
Outcome<PoliticalMilitaryAppointmentCommand> decode_political_military_appointment_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_political_military_appointment_catalog(const PoliticalMilitaryAppointmentCatalog&);
Outcome<PoliticalMilitaryAppointmentCatalog> decode_political_military_appointment_catalog(const BoundaryValue&);
BoundaryValue encode_political_military_appointments(const PoliticalMilitaryAppointmentState&);
Outcome<PoliticalMilitaryAppointmentState> decode_political_military_appointments(const BoundaryValue&);
Outcome<PoliticalMilitaryAppointmentState> decode_political_military_appointments(const BoundaryValue&,bool field_commands);
Status visit_political_military_appointments_checkpoint(CodecVisitor&,PoliticalMilitaryAppointmentState&);
Status visit_political_military_appointments_checkpoint(CodecVisitor&,PoliticalMilitaryAppointmentState&,bool field_commands);
}
