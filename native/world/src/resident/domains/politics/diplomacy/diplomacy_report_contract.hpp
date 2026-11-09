// 正式返报后继的唯一政治来源；此头不启用旧使者v1的未实现返报分支。
#pragma once
#include "diplomacy_mission_contract.hpp"
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
inline constexpr const char* diplomatic_envoy_report_fact_kind = "diplomatic_envoy_report_received";
inline constexpr const char* diplomatic_envoy_report_command_kind = "diplomatic_receive_envoy_report";

// 报告的是已实际发生的呈递经过，不声称对方接受或条约已经生效。
struct DiplomaticEnvoyReportSource {
    StableId fact_id, mission_authority_fact_id, proposal_id, proposal_fact_id;
    StableId presentation_fact_id, envoy_actor_id;
    Integer terms_revision = 0, completed_tick = 0;
    StableId task_id, contract_id, acceptance_fact_id, report_action_id;
    ActingAuthority receiver;
    StableId receiver_grant_id, report_site_id;
};
enum class DiplomaticEnvoyReportAttemptKind { reported, renew_contact, responsibility_blocked };
struct DiplomaticEnvoyReportAttempt {
    DiplomaticEnvoyReportAttemptKind kind = DiplomaticEnvoyReportAttemptKind::responsibility_blocked;
    StableId report_fact_id, code;
    std::optional<DiplomaticMissionContinuity> continuity;
};
class DiplomaticEnvoyReportRead {
public:
    virtual ~DiplomaticEnvoyReportRead() = default;
    // 只供已验证Task历史引用，须精确核其四ID与原呈递来源；不是玩家任意ID查询。
    virtual Outcome<DiplomaticEnvoyReportSource> diplomatic_envoy_report_source(
        const StableId& report_fact_id) const = 0;
};
}
