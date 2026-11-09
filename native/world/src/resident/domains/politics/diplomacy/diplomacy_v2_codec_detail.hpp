// 两个显式版本codec共用新子载荷字段；v1分支从不访问此表。
#pragma once
#include "diplomacy_v2_state.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
struct DiplomaticReportAuthorityWire { ActingAuthority& value; };
template<class A> void record(A& a, DiplomaticReportAuthorityWire& w) {
    a("organization_id", w.value.organization_id); a("office_id", w.value.office_id);
    a("authority_epoch", w.value.authority_epoch); a("actor_id", w.value.actor_id);
}
template<class A> void record(A& a, DiplomaticMissionKnowledge& v) {
    a("id", v.id); a("delegation_policy_id", v.delegation_policy_id); a("mission_authority_fact_id", v.mission_authority_fact_id); a("envoy_actor_id", v.envoy_actor_id);
    a("issuer_actor_id", v.issuer_actor_id); a("session_actor_id", v.session_actor_id); a("session_id", v.session_id);
    a("site_id", v.site_id); a("role_id", v.role_id); a("provider_actor_id", v.provider_actor_id); a("context_kind", v.context_kind); a("tick", v.tick);
}
template<class A> void record(A& a, DiplomaticEnvoyReportSource& v) {
    a("fact_id", v.fact_id); a("mission_authority_fact_id", v.mission_authority_fact_id); a("proposal_id", v.proposal_id);
    a("proposal_fact_id", v.proposal_fact_id); a("presentation_fact_id", v.presentation_fact_id); a("envoy_actor_id", v.envoy_actor_id);
    a("terms_revision", v.terms_revision); a("completed_tick", v.completed_tick); a("task_id", v.task_id);
    a("contract_id", v.contract_id); a("acceptance_fact_id", v.acceptance_fact_id); a("report_action_id", v.report_action_id);
    DiplomaticReportAuthorityWire receiver{v.receiver}; a("receiver", receiver);
    a("receiver_grant_id", v.receiver_grant_id); a("report_site_id", v.report_site_id);
}
template<class A> void record(A& a, DiplomaticReportRecord& v) {
    a("source", v.source); a("delivery_fact_id", v.delivery_fact_id); a("session_actor_id", v.session_actor_id);
    a("provider_actor_id", v.provider_actor_id); a("role_id", v.role_id); a("context_kind", v.context_kind);
}
template<class A> void record(A& a, DiplomacyV2State& v) {
    a("version", v.version); a("mission_knowledge", v.mission_knowledge); a("reports", v.reports);
}
}
