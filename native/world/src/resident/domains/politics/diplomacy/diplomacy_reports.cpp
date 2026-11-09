// 报告责任只沿已完成递交与原发令主体；目标提议过期不抹掉已做工作。
#include "diplomacy_reports.hpp"
namespace hanmo::resident {
namespace {
Outcome<DiplomaticMissionApproval> report_source(const DiplomaticPresentRead& c,
    const StableId& id, const StableId& envoy, const StableId& presentation_id) {
    const auto* m = c.state.missions.find(id);
    if (!m || m->envoy_actor_id != envoy) return {false, {}, common_failure("DiplomaticMissionNotKnown").error};
    if (c.catalog.version != 2 || c.state.version != 2) return {false, {}, common_failure("DiplomaticReportVersionRequired").error};
    DiplomaticMissionAccess read(c.common, c.organizations, c.organization_catalog, c.catalog, c.state);
    auto original = read.diplomatic_mission_source(id); if (!original.ok) return original;
    const auto presentation = read.diplomatic_presentation_source(presentation_id);
    if (!m->return_report_required || m->report_work_ticks <= 0 || !presentation.ok ||
        !presentation.value.task_binding || presentation.value.task_binding->mission_authority_fact_id != id ||
        presentation.value.proposal_id != m->proposal_id || presentation.value.proposal_fact_id != m->proposal_fact_id ||
        presentation.value.terms_revision != m->terms_revision || presentation.value.envoy_actor_id != envoy ||
        presentation.value.completed_tick < m->issued_tick || presentation.value.completed_tick >= m->delivery_expires_tick)
        return {false, {}, common_failure("InvalidDiplomaticReportDeliverySource").error};
    return original;
}
}
Outcome<DiplomaticMissionContinuity> diplomatic_report_responsibility(const DiplomaticPresentRead& c,
    const StableId& authority, const StableId& envoy, const StableId& presentation) {
    auto source = report_source(c, authority, envoy, presentation); if (!source.ok) return {false, {}, source.error};
    const auto& m = source.value; const auto* actor = c.common.actor(envoy);
    const auto* org = c.organizations.organizations.find(m.issuer_organization_id);
    using K = DiplomaticMissionContinuityKind;
    const auto result = [](K kind, const StableId& code) -> Outcome<DiplomaticMissionContinuity> { return {true, {kind, code, {}}, {}}; };
    if (!actor || !org) return {false, {}, common_failure("InvalidDiplomaticReportResponsibilitySource").error};
    if (!actor->alive) return result(K::terminate_envoy_dead, "DiplomaticEnvoyDead");
    if (org->status == OrganizationStatus::dissolved) return result(K::terminate_principal_dissolved, "DiplomaticPrincipalDissolved");
    if (!actor->available) return result(K::pause_envoy, "DiplomaticEnvoyUnavailable");
    const auto* office = c.organizations.offices.find(m.report_office_id);
    if (!office || office->organization_id != m.issuer_organization_id || office->status != OfficeStatus::active || !office->holder_actor_id)
        return result(K::needs_reassignment, "DiplomaticReportOfficeUnavailable");
    const ActingAuthority receiver{office->organization_id, office->id, office->epoch, *office->holder_actor_id};
    const auto current = check_authority(c.common, c.organizations, c.organization_catalog, receiver,
        diplomatic_envoy_report_command_kind, {ScopeRef::Kind::site, m.report_site_id}, 0);
    if (!current.ok) return result(K::needs_reassignment, "DiplomaticReportReceiverUnauthorized");
    return result(K::continue_work, {});
}
Status preview_diplomatic_report_contact(const DiplomaticPresentRead& c, const StableId& authority,
    const StableId& envoy, const StableId& presentation, const FaceToFaceContactProof& contact) {
    const auto source = report_source(c, authority, envoy, presentation); if (!source.ok) return {false, {}, source.error};
    const auto continuity = diplomatic_report_responsibility(c, authority, envoy, presentation);
    if (!continuity.ok) return {false, {}, continuity.error};
    if (continuity.value.kind != DiplomaticMissionContinuityKind::continue_work) return common_failure(continuity.value.code);
    const auto* office = c.organizations.offices.find(source.value.report_office_id);
    if (contact.actor_id != envoy || contact.target_actor_id == envoy || contact.target_actor_id != *office->holder_actor_id ||
        contact.context.place_id != source.value.report_site_id) return common_failure("DiplomaticReportContactChanged");
    return revalidate_face_to_face_contact(c.common, c.interaction, c.interaction_catalog, c.interaction_access, contact);
}
}
