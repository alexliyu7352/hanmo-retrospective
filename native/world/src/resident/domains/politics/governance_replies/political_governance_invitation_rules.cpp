// 两位原本人在同治所关闭尚未受领的邀请；不会撤销合同、释放预约或重置工作进度。
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/politics/politics_internal.hpp"

namespace hanmo::resident {
namespace {
// 报价只能复用同一候选身份；原命令仍在提交时重新核验。
bool same_identity(const WorldIdentity& a, const WorldIdentity& b) {
    return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch && a.revision == b.revision && a.tick == b.tick;
}
// 双方本人均須真正空闲、活着且同场；远方同城名义不能代替当面表达。
Status current_contact(const GovernanceInvitationReplyContext& x, const GovernanceAuthorityApproval& source) {
    for (const auto* actor : {&source.issuer_actor_id, &source.executor_actor_id}) {
        auto valid = politics_detail::available(x.common, x.activity, *actor);
        if (!valid.ok) return valid;
        const auto here = politics_detail::place(x.common, *actor);
        if (!here || here->place_id != source.site_id) return common_failure("GovernanceInvitationContactRequired", *actor);
    }
    return common_success();
}
}

// 先挡非本人及未知源，再核原业务；首片仅玩家提交，NPC接线必须另有显式producer与结果合同。
Outcome<GovernanceInvitationReplyQuote> preview_governance_invitation_reply(
    const GovernanceInvitationReplyContext& x, const StableId& actor, const GovernanceInvitationReplyCommand& command) {
    const auto id = std::visit([](const auto& value) { return value.authority_fact_id; }, command);
    const auto fail = [&](const StableId& code) { return Outcome<GovernanceInvitationReplyQuote>{false, {}, common_failure(code, id).error}; };
    if (actor != x.common.state().metadata.player_actor_id) return fail("GovernanceInvitationPlayerRequired");
    const bool decline = std::holds_alternative<GovernanceDeclineInvitation>(command);
    const auto* known = x.governance.authorities.find(id);
    if (!known || (decline ? known->executor_actor_id : known->issuer_actor_id) != actor)
        return fail("GovernanceInvitationNotKnown");
    if(!x.player_before_fact)return fail("GovernanceInvitationControlHistoryRequired");
    auto valid = validate_governance_invitation_replies(x);
    if (!valid.ok) return {false, {}, valid.error};
    auto original = read_unaccepted_governance_invitation(x, id);
    if (!original.ok) return {false, {}, original.error};
    const auto& a = original.value;
    if (a.issuer_actor_id == a.executor_actor_id) return fail("GovernanceInvitationOtherPartyRequired");
    if (x.common.tick() < a.issued_tick || x.common.tick() >= a.offer_expires_tick)
        return fail("GovernanceInvitationExpired");
    for (const auto& row : x.state.replies.rows()) if (row.second.authority_fact_id == id)
        return fail("GovernanceInvitationAlreadyClosed");
    valid = current_contact(x, a); if (!valid.ok) return {false, {}, valid.error};
    GovernanceInvitationReplyRecord r;
    r.authority_fact_id = a.authority_fact_id; r.actor_id = actor;
    r.counterpart_actor_id = decline ? a.issuer_actor_id : a.executor_actor_id;
    r.site_id = a.site_id; r.tick = x.common.tick();
    r.kind = decline ? GovernanceInvitationReplyKind::declined : GovernanceInvitationReplyKind::withdrawn;
    if (!decline) {
        const auto& authority = std::get<PoliticalWithdrawGovernanceInvitation>(command).authority;
        if (authority.actor_id != actor || authority.organization_id != a.organization_id ||
            authority.office_id != a.office_id || authority.authority_epoch != a.authority_epoch)
            return fail("GovernanceInvitationOriginalIssuerAuthorityRequired");
        const auto* grant = x.organization_catalog.grants.find(a.grant_id);
        if (!grant || std::find(grant->allowed_command_kinds.begin(), grant->allowed_command_kinds.end(), governance_withdraw_command) == grant->allowed_command_kinds.end())
            return fail("GovernanceInvitationWithdrawalGrantRequired");
        auto current = check_authority(x.common, x.organizations, x.organization_catalog, authority,
            governance_withdraw_command, {ScopeRef::Kind::site, a.site_id}, 0);
        if (!current.ok) return {false, {}, current.error};
        // 原签发Grant本身也必须仍在有效期，不能由另一份Grant代签旧邀请的撤回。
        const auto* office = x.organizations.offices.find(a.office_id);
        if (!office || std::find(office->grant_ids.begin(), office->grant_ids.end(), a.grant_id) == office->grant_ids.end() ||
            grant->office_id != a.office_id || grant->valid_from > x.common.tick() ||
            (grant->valid_until && x.common.tick() > *grant->valid_until) ||
            std::none_of(grant->scopes.begin(), grant->scopes.end(), [&](const ScopeRef& scope) {
                return scope.kind == ScopeRef::Kind::site && scope.id == a.site_id;
            })) return fail("GovernanceInvitationWithdrawalGrantRequired");
        r.grant_id = a.grant_id;
    }
    return {true, {x.common.state().identity, command, std::move(r)}, {}};
}

// 调用者同一保存点覆盖Common与本新账；所有授权重报价通过后才写唯一关闭事实。
Outcome<StableId> apply_governance_invitation_reply_in_candidate(
    CommonStore& common, GovernanceInvitationReplyState& state, const GovernanceInvitationReplyContext& x,
    const StableId& actor, const GovernanceInvitationReplyCommand& command, const GovernanceInvitationReplyQuote& quote) {
    if (&common != &x.common || &state != &x.state) return {false, {}, common_failure("GovernanceInvitationWrongCandidate").error};
    if (!same_identity(common.state().identity, quote.identity) ||
        !compare_boundary(encode_governance_invitation_reply_command(command), encode_governance_invitation_reply_command(quote.command)).exact())
        return {false, {}, common_failure("StaleGovernanceInvitationReplyQuote").error};
    auto current = preview_governance_invitation_reply(x, actor, command);
    if (!current.ok) return {false, {}, current.error};
    auto id = common.next_id(governance_invitation_reply_prefix); if (!id.ok) return id;
    auto record = std::move(current.value.source); record.id = id.value;
    auto appended = common.append_fact({id.value, governance_invitation_reply_fact_kind, governance_invitation_reply_module});
    if (!appended.ok) return {false, {}, appended.error};
    if (!state.replies.insert(id.value, std::move(record))) return {false, {}, common_failure("DuplicateGovernanceInvitationReply", id.value).error};
    return id;
}
}
