// 关闭仅针对一份真实且从未受领的邀请；原Task一旦存在，任何谢绝/撤回记录都不能追溯成立。
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
#include <set>

namespace hanmo::resident {
namespace {
// 原治理契约是接受的唯一证明，终结或取消也不会让旧邀请重新变成未受领。
Status unaccepted(const GovernanceInvitationReplyContext& x, const StableId& authority) {
    auto read = x.tasks.governance_task_for_authority(authority);
    if (!read.ok) return {false, {}, read.error};
    if (read.value) {
        if (read.value->acceptance.authority_source_fact_id != authority)
            return common_failure("GovernanceInvitationTaskSourceMismatch", authority);
        return common_failure("GovernanceInvitationAlreadyAccepted", authority);
    }
    return common_success();
}

// 这里只核双方生命前界，不声称重放旧位置或占用；当面由本关闭事实永久见证。
Status historical_life(const GovernanceInvitationReplyContext& x, const GovernanceInvitationReplyRecord& r) {
    for (const auto* actor : {&r.actor_id, &r.counterpart_actor_id}) {
        auto read = read_actor_presence_before_fact(x.common, x.common_catalog, x.death_origin, *actor, r.tick, r.id);
        if (!read.ok) return {false, {}, read.error};
        if (!read.value.alive) return common_failure("GovernanceInvitationHistoricalContactUnavailable", r.id);
    }
    return common_success();
}

// 撤回不是新的预算委任；仍须原签发者在原epoch持有同一Grant中的显式撤回词。
Status historical_withdrawal(const GovernanceInvitationReplyContext& x, const GovernanceAuthorityApproval& a,
                            const GovernanceInvitationReplyRecord& r, Integer sequence) {
    const auto* grant = x.organization_catalog.grants.find(r.grant_id);
    if (!grant || r.grant_id != a.grant_id || grant->office_id != a.office_id || grant->valid_from > r.tick ||
        (grant->valid_until && r.tick > *grant->valid_until) ||
        std::find(grant->allowed_command_kinds.begin(), grant->allowed_command_kinds.end(), governance_withdraw_command) == grant->allowed_command_kinds.end() ||
        std::none_of(grant->scopes.begin(), grant->scopes.end(), [&](const ScopeRef& scope) {
            return scope.kind == ScopeRef::Kind::site && scope.id == a.site_id;
        })) return common_failure("GovernanceInvitationWithdrawalGrantRequired", r.id);
    return validate_historical_office_signer(x.common, x.organizations, x.organization_catalog,
        {a.organization_id, a.office_id, a.authority_epoch, a.issuer_actor_id}, r.tick, sequence);
}
}

// 原批准完整来源与唯一接受账共同决定是否仍属于邀请，不借Task当前终态重新开放。
Outcome<GovernanceAuthorityApproval> read_unaccepted_governance_invitation(const GovernanceInvitationReplyContext& x, const StableId& id) {
    auto original = x.originals.governance_authority_approval(id);
    if (!original.ok) return original;
    auto valid = unaccepted(x, id);
    return valid.ok ? original : Outcome<GovernanceAuthorityApproval>{false, {}, valid.error};
}

// 新账只消费原批准和原契约；不回调接受包装器、不扫描政治接管全部责任。
Status validate_governance_invitation_replies(const GovernanceInvitationReplyContext& x) {
    if (x.state.version != 1 || x.state.replies.size() > 65536)
        return common_failure("InvalidGovernanceInvitationReplies");
    std::set<StableId> authorities;
    std::vector<StableId> order;
    Integer previous_id = 0, previous_tick = -1;
    const auto& common_order = x.common.state().facts.order();
    for (const auto& row : x.common.state().facts.rows()) {
        const auto& head = row.second;
        if (head.kind != governance_invitation_reply_fact_kind) {
            if (head.domain == governance_invitation_reply_module)
                return common_failure("UnknownGovernanceInvitationReplyFact", row.first);
            continue;
        }
        const auto sequence = organization_instance_sequence(row.first, governance_invitation_reply_prefix);
        const auto* r = x.state.replies.find(row.first);
        if (!sequence || *sequence <= previous_id || *sequence >= x.common.state().next_instance_id ||
            head.id != row.first || head.domain != governance_invitation_reply_module || !r || r->id != row.first ||
            r->tick < previous_tick || r->tick < 0 || r->tick > x.common.tick() ||
            !authorities.insert(r->authority_fact_id).second)
            return common_failure("InvalidGovernanceInvitationReplyFact", row.first);
        auto original = x.originals.governance_authority_approval(r->authority_fact_id);
        if (!original.ok) return {false, {}, original.error};
        const auto& a = original.value;
        // 原只读已先完整验证批准；代理独占前缀还须其显式AgencyRead，不能靠此序号分支自行授许可。
        auto original_id = organization_instance_sequence(a.authority_fact_id, "politics_governance_authority");
        if(!original_id)original_id=organization_instance_sequence(a.authority_fact_id,"politics_governance_agency_authority");
        const auto before = std::find(common_order.begin(), common_order.end(), a.authority_fact_id);
        const auto after = std::find(common_order.begin(), common_order.end(), r->id);
        if (!original_id || *original_id >= *sequence || before == common_order.end() || after == common_order.end() || before >= after ||
            a.authority_fact_id != r->authority_fact_id || a.issued_tick > r->tick || r->tick >= a.offer_expires_tick ||
            a.site_id != r->site_id || a.issuer_actor_id == a.executor_actor_id)
            return common_failure("GovernanceInvitationOriginalSourceMismatch", r->id);
        auto valid = unaccepted(x, a.authority_fact_id);
        if (!valid.ok) return valid;
        if (r->kind == GovernanceInvitationReplyKind::declined) {
            if (r->actor_id != a.executor_actor_id || r->counterpart_actor_id != a.issuer_actor_id || !r->grant_id.empty())
                return common_failure("GovernanceInvitationDeclineActorMismatch", r->id);
        } else if (r->kind == GovernanceInvitationReplyKind::withdrawn) {
            if (r->actor_id != a.issuer_actor_id || r->counterpart_actor_id != a.executor_actor_id)
                return common_failure("GovernanceInvitationWithdrawalActorMismatch", r->id);
            valid = historical_withdrawal(x, a, *r, *sequence);
            if (!valid.ok) return valid;
        } else return common_failure("UnknownGovernanceInvitationReplyKind", r->id);
        if(!x.player_before_fact)return common_failure("GovernanceInvitationControlHistoryRequired",r->id);
        auto player=x.player_before_fact(r->tick,r->id);if(!player.ok)return {false,{},player.error};
        if(player.value!=r->actor_id||!x.common.actor(player.value))return common_failure("GovernanceInvitationHistoricalPlayerMismatch",r->id);
        valid = historical_life(x, *r);
        if (!valid.ok) return valid;
        previous_id = *sequence; previous_tick = r->tick; order.push_back(r->id);
    }
    if (order != x.state.replies.order()) return common_failure("GovernanceInvitationReplyCoverageMismatch");
    return common_success();
}

// 关闭后的邀请仍保留全部原批准，读口仅返回真实回复，不生成另一份到期或任务状态。
Outcome<std::optional<GovernanceInvitationReplySource>> GovernanceInvitationReplyAccess::governance_invitation_reply(const StableId& authority) const {
    auto valid = validate_governance_invitation_replies(context_);
    if (!valid.ok) return {false, {}, valid.error};
    for (const auto& row : context_.state.replies.rows()) {
        const auto& r = row.second;
        if (r.authority_fact_id == authority)
            return {true, GovernanceInvitationReplySource{context_.common.state().identity, r.id, r.authority_fact_id,
                r.actor_id, r.counterpart_actor_id, r.site_id, r.kind, r.tick}, {}};
    }
    return {true, std::nullopt, {}};
}
}
