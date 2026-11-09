// 唯一Task报告Action完成时写政治接收来源，Task最终状态由S3在同候选内衔接。
#include "diplomacy_reports.hpp"
#include "resident/domains/tasks/envoy_task_report_contact.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
bool same_identity(const WorldIdentity& a, const WorldIdentity& b) {
    return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch && a.revision == b.revision && a.tick == b.tick;
}
const Grant* report_grant(const DiplomaticPresentRead& r, const OfficeState& office, const StableId& place, Integer completed) {
    for (const auto& id : office.grant_ids) {
        const auto* grant = r.organization_catalog.grants.find(id);
        if (!grant || grant->office_id != office.id || grant->amount_limit < 0 || grant->valid_from > r.common.tick() ||
            (grant->valid_until && *grant->valid_until < completed) ||
            std::find(grant->allowed_command_kinds.begin(), grant->allowed_command_kinds.end(), diplomatic_envoy_report_command_kind) == grant->allowed_command_kinds.end() ||
            std::none_of(grant->scopes.begin(), grant->scopes.end(), [&](const auto& scope) { return scope.kind == ScopeRef::Kind::site && scope.id == place; })) continue;
        bool resources = true;
        for (const auto& id : grant->wallet_ids) {
            const auto* wallet = r.common.wallet(id);
            if (!wallet || wallet->owner.kind != Owner::Kind::organization || wallet->owner.id != office.organization_id) resources = false;
        }
        for (const auto& id : grant->inventory_ids) {
            const auto* inventory = r.common.inventory(id);
            if (!inventory || inventory->owner.kind != Owner::Kind::organization || inventory->owner.id != office.organization_id) resources = false;
        }
        if (resources) return grant;
    }
    return nullptr;
}
}
Outcome<DiplomaticEnvoyReportAttempt> attempt_diplomatic_envoy_report_in_transaction(CommonStore& common, DiplomacyV2State& state,
    const DiplomaticPresentRead& read, const TaskEnvoyReportContactRead& tasks,
    const StableId& task_id, const StableId& action_id, const DynamicStepInput& step) {
    using K = DiplomaticEnvoyReportAttemptKind;
    const auto fail = [](const StableId& code) -> Outcome<DiplomaticEnvoyReportAttempt> { return {false, {}, common_failure(code).error}; };
    if (&common != &read.common || state.version != 2 || read.catalog.version != 2 || read.state.version != 2) return fail("DiplomaticReportCandidateMismatch");
    if (step.from_tick != common.tick() || step.from_tick == INT64_MAX || step.to_tick != step.from_tick + 1 ||
        std::find(step.existing_action_ids.begin(), step.existing_action_ids.end(), action_id) == step.existing_action_ids.end()) return fail("InvalidDiplomaticReportStep");
    const auto proof = tasks.envoy_report_contact(task_id, action_id, step.to_tick); if (!proof.ok) return {false, {}, proof.error};
    const auto& p = proof.value; const auto& a = p.acceptance; const auto& d = p.delivery;
    if (!same_identity(p.source_identity, common.state().identity) || p.action_id != action_id || p.completed_tick != step.to_tick || a.task_id != task_id ||
        d.task_id != task_id || d.contract_id != a.contract_id || d.acceptance_fact_id != a.fact_id) return fail("StaleDiplomaticReportProof");
    DiplomaticMissionAccess missions(common, read.organizations, read.organization_catalog, read.catalog, read.state);
    auto mission = missions.diplomatic_mission_source(a.mission_authority_fact_id); if (!mission.ok) return {false, {}, mission.error};
    const auto& m = mission.value;
    if (!a.return_report_required || a.return_report_required != m.return_report_required || a.envoy_actor_id != m.envoy_actor_id ||
        a.proposal_id != m.proposal_id || a.proposal_fact_id != m.proposal_fact_id || a.terms_revision != m.terms_revision ||
        a.issuer_organization_id != m.issuer_organization_id || a.target_organization_id != m.target_organization_id ||
        a.target_office_id != m.target_office_id || a.delivery_site_id != m.delivery_site_id ||
        a.report_office_id != m.report_office_id || a.report_site_id != m.report_site_id ||
        a.continuity_policy_id != m.continuity_policy_id || a.delivery_expires_tick != m.delivery_expires_tick)
        return fail("DiplomaticReportAcceptanceMismatch");
    auto continuity = diplomatic_report_responsibility(read, m.mission_authority_fact_id, m.envoy_actor_id, d.diplomatic_presentation_fact_id);
    if (!continuity.ok) return {false, {}, continuity.error};
    if (continuity.value.kind != DiplomaticMissionContinuityKind::continue_work)
        return {true, {K::responsibility_blocked, {}, continuity.value.code, continuity.value}, {}};
    const auto* office = read.organizations.offices.find(m.report_office_id);
    if (!office || !office->holder_actor_id || *office->holder_actor_id != p.context.provider_actor_id ||
        p.context.provider_actor_id == m.envoy_actor_id || p.context.place_id != m.report_site_id)
        return {true, {K::renew_contact, {}, "DiplomaticReportReceiverChanged", {}}, {}};
    const auto* grant = report_grant(read, *office, m.report_site_id, step.to_tick);
    if (!grant) return {true, {K::renew_contact, {}, "DiplomaticReportReceiverUnauthorized", {}}, {}};
    for (const auto& row : state.reports.rows()) if (row.second.source.task_id == task_id) {
        const auto& old = row.second.source;
        if (old.report_action_id != action_id || old.acceptance_fact_id != a.fact_id || old.completed_tick != step.to_tick) return fail("DuplicateDiplomaticEnvoyReport");
        DiplomaticReportAccess source(read, state, step); auto prior = source.diplomatic_envoy_report_source(row.first);
        if (!prior.ok) return {false, {}, prior.error}; return {true, {K::reported, row.first, {}, {}}, {}};
    }
    const auto id = common.next_id("diplomatic_report"); if (!id.ok) return {false, {}, id.error};
    DiplomaticReportRecord record; auto& out = record.source;
    out.fact_id = id.value; out.mission_authority_fact_id = m.mission_authority_fact_id; out.proposal_id = m.proposal_id; out.proposal_fact_id = m.proposal_fact_id;
    out.presentation_fact_id = d.diplomatic_presentation_fact_id; out.envoy_actor_id = m.envoy_actor_id; out.terms_revision = m.terms_revision;
    out.completed_tick = step.to_tick; out.task_id = task_id; out.contract_id = a.contract_id; out.acceptance_fact_id = a.fact_id; out.report_action_id = action_id;
    out.receiver = {office->organization_id, office->id, office->epoch, *office->holder_actor_id}; out.receiver_grant_id = grant->id; out.report_site_id = m.report_site_id;
    record.delivery_fact_id = d.fact_id; record.session_actor_id = m.envoy_actor_id; record.provider_actor_id = p.context.provider_actor_id;
    record.role_id = p.context.role_id; record.context_kind = p.context.kind;
    const auto recorded = common.append_fact({id.value, diplomatic_envoy_report_fact_kind, diplomatic_fact_domain}); if (!recorded.ok) return {false, {}, recorded.error};
    if (!state.reports.insert(id.value, std::move(record))) return fail("DuplicateDiplomaticEnvoyReport");
    return {true, {K::reported, id.value, {}, {}}, {}};
}
}
