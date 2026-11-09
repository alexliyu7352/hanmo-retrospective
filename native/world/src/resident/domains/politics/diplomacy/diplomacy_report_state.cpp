// 报告来源永久绑定原使命、实际交付与接收Office；冷读不重演返报或改旧提议期限。
#include "diplomacy_reports.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool same_identity(const WorldIdentity& a, const WorldIdentity& b) {
    return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch && a.revision == b.revision && a.tick == b.tick;
}
std::optional<Integer> sequence(const CommonStore& c, const StableId& id, const StableId& prefix,
    const StableId& domain, const StableId& kind) {
    const auto number = organization_instance_sequence(id, prefix); const auto* fact = c.state().facts.find(id);
    if (!number || *number >= c.state().next_instance_id || !fact || fact->id != id || fact->domain != domain || fact->kind != kind) return {};
    return number;
}
}
Outcome<DiplomaticEnvoyReportSource> DiplomaticReportAccess::diplomatic_envoy_report_source(const StableId& id) const {
    const auto fail = [&](const StableId& code) -> Outcome<DiplomaticEnvoyReportSource> { return {false, {}, common_failure(code, id).error}; };
    const auto& c = read_.common;
    if (reports_.version != 2 || read_.catalog.version != 2 || read_.state.version != 2) return fail("DiplomaticReportVersionRequired");
    Integer through = c.tick();
    if (phase_) {
        if (!same_identity(phase_identity_, c.state().identity) || phase_->from_tick != c.tick() || phase_->from_tick == INT64_MAX ||
            phase_->to_tick != phase_->from_tick + 1) return fail("InvalidDiplomaticReportPhaseRead");
        through = phase_->to_tick;
    }
    const auto* record = reports_.reports.find(id); if (!record || record->source.fact_id != id) return fail("InvalidDiplomaticReportSource");
    const auto& r = record->source;
    DiplomaticMissionAccess missions(c, read_.organizations, read_.organization_catalog, read_.catalog, read_.state);
    const auto mission = missions.diplomatic_mission_source(r.mission_authority_fact_id);
    const auto presentation = missions.diplomatic_presentation_source(r.presentation_fact_id);
    const auto n = sequence(c, id, "diplomatic_report", "politics", diplomatic_envoy_report_fact_kind);
    const auto accepted = sequence(c, r.acceptance_fact_id, "envoy_accept", "tasks", "envoy_task_accepted");
    const auto delivered = sequence(c, record->delivery_fact_id, "envoy_delivery", "tasks", "envoy_task_delivered");
    const auto presented = sequence(c, r.presentation_fact_id, "diplomatic_presentation", "politics", diplomatic_presentation_fact_kind);
    if (!mission.ok || !presentation.ok || !presentation.value.task_binding || !n || !accepted || !delivered || !presented ||
        *accepted >= *presented || *presented >= *delivered || *delivered >= *n || r.completed_tick > through || r.completed_tick < 0)
        return fail("InvalidDiplomaticReportSource");
    const auto& m = mission.value; const auto& p = presentation.value; const auto& task = *p.task_binding;
    if (!m.return_report_required || m.report_work_ticks <= 0 || r.proposal_id != m.proposal_id || r.proposal_fact_id != m.proposal_fact_id ||
        r.terms_revision != m.terms_revision || r.envoy_actor_id != m.envoy_actor_id || r.receiver.actor_id == r.envoy_actor_id ||
        r.receiver.organization_id != m.issuer_organization_id || r.receiver.office_id != m.report_office_id || r.report_site_id != m.report_site_id ||
        r.task_id != task.task_id || r.contract_id != task.contract_id || r.acceptance_fact_id != task.acceptance_fact_id ||
        r.mission_authority_fact_id != task.mission_authority_fact_id || r.report_action_id.empty() || r.report_action_id == task.action_id ||
        r.completed_tick < p.completed_tick || p.completed_tick >= m.delivery_expires_tick ||
        record->session_actor_id != r.envoy_actor_id || record->provider_actor_id != r.receiver.actor_id)
        return fail("DiplomaticReportBindingMismatch");
    InteractionContext context{r.report_site_id, record->provider_actor_id, record->role_id}; context.kind = record->context_kind;
    if (!face_to_face_contact_context_valid(context)) return fail("InvalidDiplomaticReportContact");
    const auto* grant = read_.organization_catalog.grants.find(r.receiver_grant_id);
    const auto* office = read_.organization_catalog.offices.find(r.receiver.office_id);
    if (!grant || !office || grant->office_id != r.receiver.office_id || grant->amount_limit < 0 ||
        std::find(office->grant_ids.begin(), office->grant_ids.end(), grant->id) == office->grant_ids.end() ||
        r.completed_tick < grant->valid_from || (grant->valid_until && r.completed_tick > *grant->valid_until) ||
        std::find(grant->allowed_command_kinds.begin(), grant->allowed_command_kinds.end(), diplomatic_envoy_report_command_kind) == grant->allowed_command_kinds.end() ||
        std::none_of(grant->scopes.begin(), grant->scopes.end(), [&](const auto& scope) { return scope.kind == ScopeRef::Kind::site && scope.id == r.report_site_id; }))
        return fail("InvalidDiplomaticReportAuthority");
    if (r.completed_tick <= c.tick()) {
        auto history = validate_historical_office_signer(c, read_.organizations, read_.organization_catalog, r.receiver, r.completed_tick, *n);
        if (!history.ok) return {false, {}, history.error};
    } else {
        if (!phase_ || r.completed_tick != phase_->to_tick) return fail("InvalidDiplomaticReportPhaseRead");
        const auto* action = c.action(r.report_action_id);
        if (!action || action->kind != "envoy_report" || action->actor_id != r.envoy_actor_id || action->remaining_ticks != 0 ||
            std::find(phase_->existing_action_ids.begin(), phase_->existing_action_ids.end(), r.report_action_id) == phase_->existing_action_ids.end())
            return fail("InvalidDiplomaticReportPhaseRead");
        auto current = check_authority(c, read_.organizations, read_.organization_catalog, r.receiver, diplomatic_envoy_report_command_kind,
            {ScopeRef::Kind::site, r.report_site_id}, 0);
        if (!current.ok) return {false, {}, current.error};
    }
    return {true, r, {}};
}
Status validate_diplomatic_report_state(const DiplomaticHistoryRead& read, const DiplomacyV2State& state) {
    if (state.version != 2 || read.catalog.version != 2 || read.state.version != 2) return common_failure("DiplomaticReportVersionRequired");
    DiplomaticReportAccess reports(read, state); std::set<StableId> tasks;
    for (const auto& row : state.reports.rows()) {
        auto source = reports.diplomatic_envoy_report_source(row.first); if (!source.ok) return {false, {}, source.error};
        if (!tasks.insert(source.value.task_id).second) return common_failure("DuplicateDiplomaticEnvoyReport", source.value.task_id);
    }
    for (const auto& row : read.common.state().facts.rows()) if (row.second.domain == "politics" &&
        row.second.kind == diplomatic_envoy_report_fact_kind && !state.reports.find(row.first)) return common_failure("MissingDiplomaticReportPayload", row.first);
    return common_success();
}
}
