// 新邀请回复只阻新接受；原批准、工作与原赈济连续性没有另一份真值。
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {
// 旧不可变批准始终可读，保证已成立任务的原金额/双方/工作来源不被关闭表代写。
Outcome<GovernanceAuthorityApproval> GovernanceInvitationAcceptanceAccess::governance_authority_approval(const StableId& id) const {
    return original_.governance_authority_approval(id);
}
// 原本人知情及全部接受规则先行；只有同候选真实原关闭事实可以加拒绝。
Outcome<GovernanceAuthorityApproval> GovernanceInvitationAcceptanceAccess::governance_authority_for_acceptance(
    const StableId& id, const StableId& actor, Integer revision) const {
    auto approved = original_.governance_authority_for_acceptance(id, actor, revision);
    if (!approved.ok) return approved;
    auto reply = replies_.governance_invitation_reply(id);
    if (!reply.ok) return {false, {}, reply.error};
    if (!reply.value) return approved;
    const auto& r = *reply.value; const auto& a = approved.value;
    const auto& identity = common_.state().identity; const auto* h = common_.state().facts.find(r.fact_id);
    const auto sequence = organization_instance_sequence(r.fact_id, governance_invitation_reply_prefix);
    const bool declined = r.kind == GovernanceInvitationReplyKind::declined;
    if (r.source_identity.world_id != identity.world_id || r.source_identity.execution_epoch != identity.execution_epoch ||
        r.source_identity.revision != identity.revision || r.source_identity.tick != identity.tick ||
        r.authority_fact_id != id || r.site_id != a.site_id || r.tick < a.issued_tick || r.tick > common_.tick() ||
        r.tick >= a.offer_expires_tick || !sequence || *sequence >= common_.state().next_instance_id ||
        !h || h->id != r.fact_id || h->kind != governance_invitation_reply_fact_kind || h->domain != governance_invitation_reply_module ||
        (r.kind != GovernanceInvitationReplyKind::declined && r.kind != GovernanceInvitationReplyKind::withdrawn) ||
        r.actor_id != (declined ? a.executor_actor_id : a.issuer_actor_id) ||
        r.counterpart_actor_id != (declined ? a.issuer_actor_id : a.executor_actor_id))
        return {false, {}, common_failure("GovernanceInvitationClosureSourceMismatch", id).error};
    return {false, {}, common_failure(declined ? "GovernanceInvitationDeclined" : "GovernanceInvitationWithdrawn", id).error};
}
// 原已受领工作不使用邀请窗口，也不因另一份新邀请回复变成自动取消。
Outcome<GovernanceContinuityResult> GovernanceInvitationAcceptanceAccess::governance_continuity(const StableId& id, const StableId& actor) const {
    return original_.governance_continuity(id, actor);
}
// 真实资源清算仍消费原接管决定与冻结赈济契约，不经过未受领关闭判定。
Outcome<GovernanceContinuityResult> GovernanceInvitationAcceptanceAccess::governance_relief_continuity(
    const StableId& id, const StableId& actor, const StableId& contract, const PoliticalTakeoverRead& takeover) const {
    return original_.governance_relief_continuity(id, actor, contract, takeover);
}
}
